/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const { CookieXPCShellUtils } = ChromeUtils.importESModule(
  "resource://testing-common/CookieXPCShellUtils.sys.mjs"
);

CookieXPCShellUtils.init(this);

let gHits = 0;

add_task(async function () {
  do_get_profile();

  info("Disable predictor and accept all");
  Services.prefs.setBoolPref("network.predictor.enabled", false);
  Services.prefs.setBoolPref("network.predictor.enable-prefetch", false);
  Services.prefs.setBoolPref("network.http.rcwn.enabled", false);
  Services.prefs.setIntPref("network.cookie.cookieBehavior", 0);

  const server = CookieXPCShellUtils.createServer({
    hosts: ["example.org", "foo.com", "bar.com"],
  });

  server.registerFile(
    "/font.woff",
    do_get_file("data/font.woff"),
    (_, response) => {
      response.setHeader("Access-Control-Allow-Origin", "*", false);
      gHits++;
    }
  );

  server.registerPathHandler("/font", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    let body = `
      <style type="text/css">
        @font-face {
          font-family: foo;
          src: url("http://example.org/font.woff") format('woff');
        }
        body { font-family: foo }
      </style>
      <iframe src="http://example.org/font-iframe">
      </iframe>`;
    response.bodyOutputStream.write(body, body.length);
  });

  server.registerPathHandler("/font-iframe", (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html", false);
    let body = `
      <style type="text/css">
        @font-face {
          font-family: foo;
          src: url("http://example.org/font.woff") format('woff');
        }
        body { font-family: foo }
      </style>`;
    response.bodyOutputStream.write(body, body.length);
  });

  const hitsCount = 5;

  info("Clear network caches");
  Services.cache2.clear();

  info("Reset the hits count");
  gHits = 0;

  info("Let's load a page with origin A");
  let contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://example.org/font"
  );
  await contentPage.close();

  info("Let's load a page with origin B");
  contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://foo.com/font"
  );
  await contentPage.close();

  info("Let's load a page with origin C");
  contentPage = await CookieXPCShellUtils.loadContentPage(
    "http://bar.com/font"
  );
  await contentPage.close();

  Assert.equal(gHits, hitsCount, "The number of hits match");
});
