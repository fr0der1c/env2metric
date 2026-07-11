#!/usr/bin/env bash
set -euo pipefail

BASE_URL="${SIGNOZ_BASE_URL:?SIGNOZ_BASE_URL is required, for example http://signoz-host:8080}"
DASHBOARD_FILE="${1:-dashboards/env2metric-air-quality.json}"

if [[ -z "${SIGNOZ_TOKEN:-}" ]]; then
  echo "SIGNOZ_TOKEN is required. Copy AUTH_TOKEN from SigNoz browser localStorage." >&2
  exit 2
fi

python3 - "$SIGNOZ_TOKEN" <<'PY'
import base64
import json
import sys
import time

token = sys.argv[1]
parts = token.split(".")
if len(parts) != 3:
    print("SIGNOZ_TOKEN does not look like a JWT.", file=sys.stderr)
    sys.exit(2)

payload = parts[1]
payload += "=" * (-len(payload) % 4)
try:
    claims = json.loads(base64.urlsafe_b64decode(payload))
except Exception as error:
    print(f"Failed to decode SIGNOZ_TOKEN: {error}", file=sys.stderr)
    sys.exit(2)

exp = claims.get("exp")
if isinstance(exp, int) and exp <= int(time.time()):
    print("SIGNOZ_TOKEN is expired. Copy a fresh AUTH_TOKEN from SigNoz localStorage.", file=sys.stderr)
    sys.exit(2)
PY

curl --fail-with-body \
  -sS \
  -X POST \
  "${BASE_URL}/api/v1/dashboards" \
  -H "Authorization: Bearer ${SIGNOZ_TOKEN}" \
  -H "Content-Type: application/json" \
  --data-binary "@${DASHBOARD_FILE}"
