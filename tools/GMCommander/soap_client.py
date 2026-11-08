"""TrinityCore SOAP istemcisi (sadece standart kutuphane)."""
import base64
import re
import urllib.request
import urllib.error
import html
from xml.sax.saxutils import escape

ENVELOPE = (
    '<?xml version="1.0" encoding="utf-8"?>'
    '<SOAP-ENV:Envelope xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/" '
    'xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/" '
    'xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance" '
    'xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:ns1="urn:TC">'
    '<SOAP-ENV:Body><ns1:executeCommand><command>{cmd}</command></ns1:executeCommand>'
    '</SOAP-ENV:Body></SOAP-ENV:Envelope>'
)


class SoapError(Exception):
    pass


class SoapClient:
    def __init__(self, host="127.0.0.1", port=7878, user="", password="", timeout=10):
        self.host, self.port = host, int(port)
        self.user, self.password = user, password
        self.timeout = timeout

    def execute(self, command: str) -> str:
        command = command.strip()
        if command.startswith(".") or command.startswith("!"):
            command = command[1:]
        if not command:
            raise SoapError("Bos komut")
        body = ENVELOPE.format(cmd=escape(command)).encode("utf-8")
        auth = base64.b64encode(f"{self.user}:{self.password}".encode("utf-8")).decode()
        req = urllib.request.Request(
            f"http://{self.host}:{self.port}/",
            data=body,
            headers={
                "Content-Type": "text/xml; charset=utf-8",
                "Authorization": "Basic " + auth,
                "SOAPAction": "urn:TC#executeCommand",
            },
            method="POST",
        )
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as resp:
                text = resp.read().decode("utf-8", "replace")
        except urllib.error.HTTPError as e:
            text = e.read().decode("utf-8", "replace")
            if e.code == 401:
                raise SoapError("Yetkisiz (401): kullanici adi/sifre yanlis veya GM seviyesi 3'ten dusuk.")
            fault = _tag(text, "faultstring")
            detail = _tag(text, "detail")
            if fault or detail:
                raise SoapError((fault or "") + ("\n" + detail if detail else ""))
            raise SoapError(f"HTTP {e.code}: {e.reason}")
        except urllib.error.URLError as e:
            raise SoapError(f"Baglanti kurulamadi ({self.host}:{self.port}): {e.reason}")
        except OSError as e:
            raise SoapError(f"Baglanti hatasi: {e}")

        result = _tag(text, "result")
        if result is not None:
            return result
        fault = _tag(text, "faultstring")
        if fault is not None:
            raise SoapError(fault + "\n" + (_tag(text, "detail") or ""))
        return text


def _tag(xml: str, name: str):
    m = re.search(rf"<(?:\w+:)?{name}[^>]*>(.*?)</(?:\w+:)?{name}>", xml, re.S)
    if not m:
        return None
    return html.unescape(m.group(1)).replace("\r\n", "\n").replace("\r", "\n")
