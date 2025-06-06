import asyncio

import pytest

URL = "https://www.centerparcs.ie/"

UNSUPPORTED_CSS = "#unsupported-browser-message"


@pytest.mark.asyncio
@pytest.mark.with_interventions
async def test_enabled(client):
    await client.navigate(URL)
    await asyncio.sleep(1)
    assert client.find_css(UNSUPPORTED_CSS, is_displayed=False)


@pytest.mark.asyncio
@pytest.mark.without_interventions
async def test_disabled(client):
    await client.navigate(URL)
    assert client.await_css(UNSUPPORTED_CSS, is_displayed=True)
