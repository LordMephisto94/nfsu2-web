// A browser's check of "Your copy of the game" (web/src/app/own-music-ui.ts) against a running --dev server:
//   node web/tools/own-music-e2e.mjs --base http://127.0.0.1:8641 --folder "<game folder>/SDATA"
// Picks the folder through the page's own button (the file input a browser without a directory picker gets), waits
// for the copy, presses Play and reads the game's log: the I/O worker must say it reads the player's own copy.
import { chromium } from 'playwright';
import { pickBrowser } from './browser.mjs';
const arg = n => { const i = process.argv.indexOf(n); return i < 0 ? null : process.argv[i + 1]; };
const base = arg('--base') ?? 'http://127.0.0.1:8641', folder = arg('--folder');
if (!folder) { console.error('--folder <a folder with the music> is needed'); process.exit(2); }
const browser = await chromium.launch({ ...pickBrowser(), headless: true,
  args: ['--enable-unsafe-webgpu', '--enable-features=Vulkan', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--autoplay-policy=no-user-gesture-required'] });
const page = await (await browser.newContext({ viewport: { width: 1440, height: 900 } })).newPage();
const fail = async why => { console.log('FAIL', why); await browser.close(); process.exit(1); };
try {
  // the file input's way of picking a folder: Playwright cannot answer the directory picker
  await page.addInitScript(() => { delete globalThis.showDirectoryPicker; });
  await page.goto(base, { waitUntil: 'load' });
  await page.click('#settings > .tabs > .tab[data-tab="game"]');
  const row = page.locator('.set.own-music');
  await row.waitFor({ timeout: 15000 });
  const chooser = page.waitForEvent('filechooser');
  await row.locator('button', { hasText: 'Choose game folder' }).click();
  await (await chooser).setFiles(folder);
  await page.waitForFunction(() => /Your own songs play/.test(document.querySelector('#set-own-music-state')?.textContent ?? ''), null, { timeout: 180000 })
    .catch(async () => fail(`no copy: "${await page.locator('#set-own-music-state').textContent().catch(() => '')}"`));
  const state = await page.locator('#set-own-music-state').textContent();
  const kept = await page.evaluate(async () => {
    const d = await (await navigator.storage.getDirectory()).getDirectoryHandle('nfsu2-own-music'), out = {};
    for await (const [n, h] of d.entries()) out[n] = (await h.getFile()).size;
    return out;
  });
  console.log('copied:', state, JSON.stringify(kept));
  await page.waitForFunction(() => !document.querySelector('#play').disabled, null, { timeout: 120000 });
  await page.click('#play');
  await page.waitForFunction(() => /\[data\] music: /.test(window.__nfsu2?.log?.text?.() ?? ''), null, { timeout: 120000 })
    .catch(async () => fail('the game\'s log has no music line'));
  // let the game run into its menus (the music starts there)
  await page.waitForTimeout(25000);
  const log = await page.evaluate(() => window.__nfsu2.log.text());
  const lines = log.split('\n').filter(l => /\[data\] music|NFSU2_MUSIC|TRAP|exit code/.test(l));
  console.log(lines.join('\n'));
  const own = lines.some(l => /\[data\] music: your own copy of the game's/.test(l)), bad = lines.some(l => /TRAP|did not open/.test(l));
  console.log(own && !bad ? 'PASS the game reads the player\'s own copy' : 'FAIL');
  await browser.close();
  process.exit(own && !bad ? 0 : 1);
} catch (e) { await fail(String(e)); }
