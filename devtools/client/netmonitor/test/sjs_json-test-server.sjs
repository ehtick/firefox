/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function handleRequest(request, response) {
  response.setStatusLine(request.httpVersion, 200, "OK");
  response.setHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response.setHeader("Pragma", "no-cache");
  response.setHeader("Expires", "0");

  response.setHeader("Content-Type", "application/json; charset=utf-8", false);

  // This server checks the name parameter from the request to decide which JSON object to
  // return.
  const params = request.queryString.split("&");
  const name = (params.filter(s => s.includes("name="))[0] || "").split("=")[1];
  switch (name) {
    case "null":
      response.write('{ "greeting": null }');
      break;
    case "root-null":
      response.write(`null`);
      break;
    case "nogrip":
      response.write('{"obj": {"type": "string" }}');
      break;
    case "empty":
      response.write("{}");
      break;
    case "numbers":
      response.write(`{
        "small": 12,
        "negzero": -0,
        "big": 1516340399466235648,
        "precise": 3.141592653589793238462643383279,
        "exp": 1e2
      }`);
      break;
    case "large-root-integer":
      response.write(`1516340399466235648`);
      break;
  }
}
