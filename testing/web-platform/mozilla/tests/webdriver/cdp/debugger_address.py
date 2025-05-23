# META: timeout=long

import json

import pytest
from support.context import using_context
from support.helpers import clear_pref
from tests.support.http_request import HTTPRequest


@pytest.fixture(autouse=True)
def clear_protocol_pref(session):
    yield
    clear_pref(session, "remote.active-protocols")


@pytest.mark.allow_system_access
@pytest.mark.capabilities(
    {
        "moz:firefoxOptions": {
            "prefs": {
                "remote.active-protocols": 2,
            },
        },
    }
)
def test_debugger_address_not_set(session, capabilities):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is None


@pytest.mark.allow_system_access
@pytest.mark.capabilities(
    {
        "moz:debuggerAddress": False,
        "moz:firefoxOptions": {
            "prefs": {
                "remote.active-protocols": 2,
            },
        },
    }
)
def test_debugger_address_false(session):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is None


@pytest.mark.allow_system_access
@pytest.mark.capabilities(
    {
        "moz:debuggerAddress": True,
        "moz:firefoxOptions": {
            "prefs": {
                "remote.active-protocols": 2,
            },
        },
    }
)
@pytest.mark.parametrize("fission_enabled", [True, False], ids=["enabled", "disabled"])
def test_debugger_address_true_with_fission(session, capabilities, fission_enabled):
    debugger_address = session.capabilities.get("moz:debuggerAddress")
    assert debugger_address is not None

    host, port = debugger_address.split(":")
    assert host == "127.0.0.1"
    assert port.isnumeric()

    # Fetch the browser version via the debugger address, `localhost` has
    # to work as well.
    for target_host in [host, "localhost"]:
        print(f"Connecting to WebSocket via host {target_host}")
        http = HTTPRequest(target_host, int(port))
        with http.get("/json/version") as response:
            data = json.loads(response.read())
            assert session.capabilities["browserVersion"] in data["Browser"]

    # Ensure Fission is not disabled (bug 1813981)
    with using_context(session, "chrome"):
        assert (
            session.execute_script("""return Services.appinfo.fissionAutostart""")
            is fission_enabled
        )
