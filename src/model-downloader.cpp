#include "model-downloader.h"

#include <obs-module.h>
#include <plugin-support.h>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProgressBar>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <array>
#include <filesystem>

namespace {

/* ---------------------------------------------------------------- */
/* Preset catalog                                                   */

const std::array<ModelPreset, 3> kPresets = {{
	{
		"en-20m",
		"English (20M, fast)",
		"sherpa-onnx streaming Zipformer, English-only, ~70 MB.",
		"https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
		"sherpa-onnx-streaming-zipformer-en-20M-2023-02-17.tar.bz2",
		"sherpa-onnx-streaming-zipformer-en-20M-2023-02-17",
	},
	{
		"zh-en-bilingual",
		"Chinese + English (bilingual)",
		"sherpa-onnx streaming Zipformer, Chinese+English mixed, ~300 MB.",
		"https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
		"sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20.tar.bz2",
		"sherpa-onnx-streaming-zipformer-bilingual-zh-en-2023-02-20",
	},
	{
		"en-small",
		"English (tiny, lowest resource)",
		"sherpa-onnx streaming Zipformer small, English, ~40 MB.",
		"https://github.com/k2-fsa/sherpa-onnx/releases/download/asr-models/"
		"sherpa-onnx-streaming-zipformer-small-en-2023-06-26.tar.bz2",
		"sherpa-onnx-streaming-zipformer-small-en-2023-06-26",
	},
}};

} /* namespace */

const ModelPreset *model_presets(size_t *out_count)
{
	if (out_count)
		*out_count = kPresets.size();
	return kPresets.data();
}

std::string models_root_dir()
{
	/* OBS exposes module-specific config paths via obs_module_config_path,
	 * which resolves to <OBS_CONFIG>/plugin_config/<module>/. */
	char *base = obs_module_config_path("models");
	if (!base)
		return {};
	std::string root(base);
	bfree(base);
	std::error_code ec;
	std::filesystem::create_directories(root, ec);
	return root;
}

std::string preset_install_dir(const ModelPreset &preset)
{
	std::string root = models_root_dir();
	if (root.empty())
		return {};
	return root + "/" + preset.id;
}

bool preset_is_installed(const ModelPreset &preset)
{
	const std::string dir = preset_install_dir(preset);
	if (dir.empty())
		return false;
	namespace fs = std::filesystem;
	std::error_code ec;
	if (!fs::exists(fs::path(dir) / "tokens.txt", ec))
		return false;
	/* Any encoder/decoder/joiner matching the loose family name is ok. */
	for (const auto &entry : fs::directory_iterator(dir, ec)) {
		const auto name = entry.path().filename().string();
		if (name.find("encoder") != std::string::npos)
			return true;
	}
	return false;
}

/* ---------------------------------------------------------------- */
/* Dialog                                                           */

namespace {

class DownloadDialog : public QDialog {
public:
	DownloadDialog(QWidget *parent, std::string *out_dir)
		: QDialog(parent), out_dir_(out_dir)
	{
		setWindowTitle(tr("AI Captions — Download ASR model"));
		setModal(true);

		auto *layout = new QVBoxLayout(this);
		auto *intro = new QLabel(tr(
			"Choose a model to download. Models are stored in your OBS plugin "
			"config directory and reused across sessions."));
		intro->setWordWrap(true);
		layout->addWidget(intro);

		combo_ = new QComboBox(this);
		for (const auto &p : kPresets) {
			const QString label =
				QString("%1 — %2")
					.arg(p.display_name)
					.arg(p.description);
			combo_->addItem(label, QString::fromUtf8(p.id));
		}
		layout->addWidget(combo_);

		status_ = new QLabel(tr("Idle."), this);
		layout->addWidget(status_);

		progress_ = new QProgressBar(this);
		progress_->setRange(0, 0); /* indeterminate until we have totals */
		progress_->hide();
		layout->addWidget(progress_);

		buttons_ = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
		download_btn_ = buttons_->addButton(tr("Download"),
						   QDialogButtonBox::AcceptRole);
		layout->addWidget(buttons_);

		connect(download_btn_, &QPushButton::clicked, this,
			&DownloadDialog::start);
		connect(buttons_, &QDialogButtonBox::rejected, this,
			&QDialog::reject);

		resize(520, 220);
	}

private:
	void start()
	{
		const int idx = combo_->currentIndex();
		if (idx < 0 || static_cast<size_t>(idx) >= kPresets.size())
			return;
		const ModelPreset &preset = kPresets[idx];

		download_btn_->setEnabled(false);
		combo_->setEnabled(false);
		progress_->show();
		status_->setText(tr("Downloading %1 ...").arg(preset.display_name));

		std::string install_dir = preset_install_dir(preset);
		if (install_dir.empty()) {
			fail(tr("Could not resolve model directory"));
			return;
		}

		std::filesystem::create_directories(install_dir);
		archive_path_ =
			QString::fromStdString(install_dir) + QStringLiteral(".tar.bz2");
		install_dir_ = QString::fromStdString(install_dir);
		extracted_subdir_ = QString::fromUtf8(preset.extracted_subdir);

		QNetworkRequest req(QUrl(QString::fromUtf8(preset.archive_url)));
		req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
				 QNetworkRequest::NoLessSafeRedirectPolicy);
		reply_ = nam_.get(req);

		connect(reply_, &QNetworkReply::downloadProgress, this,
			&DownloadDialog::on_progress);
		connect(reply_, &QNetworkReply::finished, this,
			&DownloadDialog::on_finished);
	}

	void on_progress(qint64 received, qint64 total)
	{
		if (total > 0) {
			progress_->setRange(0, 100);
			progress_->setValue(static_cast<int>(received * 100 / total));
			status_->setText(tr("Downloading ... %1 / %2 MB")
						 .arg(received / (1024 * 1024))
						 .arg(total / (1024 * 1024)));
		}
	}

	void on_finished()
	{
		if (!reply_)
			return;
		if (reply_->error() != QNetworkReply::NoError) {
			fail(tr("Download failed: %1").arg(reply_->errorString()));
			reply_->deleteLater();
			reply_ = nullptr;
			return;
		}

		QFile f(archive_path_);
		if (!f.open(QIODevice::WriteOnly)) {
			fail(tr("Cannot write archive to %1").arg(archive_path_));
			reply_->deleteLater();
			reply_ = nullptr;
			return;
		}
		f.write(reply_->readAll());
		f.close();
		reply_->deleteLater();
		reply_ = nullptr;

		status_->setText(tr("Extracting..."));
		progress_->setRange(0, 0);

		QProcess tar;
		QStringList args;
		args << "xjf" << archive_path_ << "-C"
		     << QFileInfo(archive_path_).absolutePath() << "--strip-components=1";
		tar.start("tar", args);
		tar.waitForFinished(-1);

		QFile::remove(archive_path_);

		if (tar.exitStatus() != QProcess::NormalExit || tar.exitCode() != 0) {
			fail(tr("Failed to extract archive: %1").arg(tar.readAllStandardError()));
			return;
		}

		if (out_dir_)
			*out_dir_ = install_dir_.toStdString();
		status_->setText(tr("Done."));
		accept();
	}

	void fail(const QString &msg)
	{
		obs_log(LOG_WARNING, "model download failed: %s",
			msg.toUtf8().constData());
		status_->setText(msg);
		progress_->hide();
		combo_->setEnabled(true);
		download_btn_->setEnabled(true);
	}

	std::string *out_dir_;
	QComboBox *combo_;
	QLabel *status_;
	QProgressBar *progress_;
	QDialogButtonBox *buttons_;
	QPushButton *download_btn_;
	QNetworkAccessManager nam_;
	QNetworkReply *reply_ = nullptr;
	QString archive_path_;
	QString install_dir_;
	QString extracted_subdir_;
};

} /* namespace */

bool show_model_download_dialog(void *parent_widget, std::string *out_model_dir)
{
	auto *parent = static_cast<QWidget *>(parent_widget);
	DownloadDialog dlg(parent, out_model_dir);
	return dlg.exec() == QDialog::Accepted;
}
