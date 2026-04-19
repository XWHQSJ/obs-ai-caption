#!/usr/bin/env node
/**
 * Explore OBS Forum resource-submission flow in a real browser session and
 * keep the window open so the user can authenticate once, then drive the
 * form with pre-filled data.
 *
 * Usage:
 *   node scripts/obs-forum-submit.mjs
 *
 * What it does:
 *   1. Launches Chromium (persistent profile under ~/.obs-forum-profile/ so
 *      your login survives between runs).
 *   2. Navigates to the Plugins category. The script pauses and tells you
 *      to sign in, then hit Enter in the terminal.
 *   3. Clicks "Add resource", fills Title / Tag line / Description /
 *      Version / External URL from SUBMISSION_DATA below.
 *   4. Leaves the final Submit click to you, so you can review.
 *
 * Requires:
 *   npm i -g playwright
 *   (chromium is installed under ~/Library/Caches/ms-playwright)
 */
import { chromium } from 'playwright';
import { readFileSync } from 'fs';
import readline from 'readline';
import { fileURLToPath } from 'url';
import { dirname, resolve } from 'path';

const __dirname = dirname(fileURLToPath(import.meta.url));
const repoRoot = resolve(__dirname, '..');

const forumDesc = readFileSync(resolve(repoRoot, 'docs/forum-description.md'), 'utf8');

const SUBMISSION_DATA = {
  title: 'AI Captions — on-device streaming ASR with sensitive-word mute',
  tagline: 'Free, offline, bilingual (EN + 中文) AI captions powered by sherpa-onnx',
  version: '0.1.0',
  external_url: 'https://github.com/XWHQSJ/obs-ai-caption/releases/tag/0.1.0',
  description: forumDesc,
};

function promptEnter(msg) {
  const rl = readline.createInterface({ input: process.stdin, output: process.stdout });
  return new Promise((resolve) => rl.question(`${msg}\n[press Enter] `, () => { rl.close(); resolve(); }));
}

async function main() {
  const userDataDir = process.env.OBS_FORUM_PROFILE || resolve(process.env.HOME, '.obs-forum-profile');
  console.log(`Launching Chromium with profile: ${userDataDir}`);
  const ctx = await chromium.launchPersistentContext(userDataDir, {
    headless: false,
    viewport: { width: 1280, height: 900 },
    args: ['--disable-blink-features=AutomationControlled'],
  });
  const page = ctx.pages()[0] ?? await ctx.newPage();

  console.log('Step 1/3 → open Plugins category page');
  await page.goto('https://obsproject.com/forum/resources/categories/obs-studio-plugins.6/', { waitUntil: 'networkidle' });

  await promptEnter('Sign in using the Log In button in the top right. When you land back on the plugins list,');

  console.log('Step 2/3 → clicking "Add resource"');
  const addBtn = page.getByRole('link', { name: /Add resource/i });
  if ((await addBtn.count()) === 0) {
    console.error('Could not find "Add resource" link. Are you logged in?');
    await promptEnter('Fix manually (log in / accept ToS) and then');
  }
  await addBtn.first().click();
  await page.waitForLoadState('networkidle');

  await promptEnter('If a sub-category chooser shows up, pick the one that fits (usually "Plugins"), then');

  console.log('Step 3/3 → filling submission form');
  try {
    await page.getByLabel(/Title/i).first().fill(SUBMISSION_DATA.title);
    await page.getByLabel(/Tag[ -]?line|Short description/i).first().fill(SUBMISSION_DATA.tagline);
    await page.getByLabel(/^Version$/i).first().fill(SUBMISSION_DATA.version);
    // External URL radio + field
    const externalRadio = page.getByLabel(/External (URL|site)/i).first();
    if (await externalRadio.count()) await externalRadio.check();
    const externalUrl = page.getByLabel(/External (download|resource) URL/i).first();
    if (await externalUrl.count()) await externalUrl.fill(SUBMISSION_DATA.external_url);
    // Rich text area: XenForo uses a BB/markdown editor. Paste plain markdown.
    const descField = page.locator('textarea[name="message"], .fr-element[contenteditable="true"]').first();
    await descField.fill(SUBMISSION_DATA.description);
  } catch (e) {
    console.warn('Some fields could not be auto-filled; please finish by hand.', e.message);
  }

  console.log('\nForm pre-filled. Review carefully, attach screenshots / GIF, then click Submit.');
  await promptEnter('When you are done (or want to bail),');
  await ctx.close();
}

main().catch((e) => { console.error(e); process.exit(1); });
