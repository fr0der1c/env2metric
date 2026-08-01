#!/usr/bin/env bash
set -euo pipefail

# 环境变量未设置时交互式收集必需参数，API Key 输入不会回显。
prompt_required_value() {
  local variable_name="$1"
  local prompt="$2"
  local secret="${3:-false}"
  local value="${!variable_name:-}"

  if [[ -z "$value" ]]; then
    if [[ "$secret" == "true" ]]; then
      read -r -s -p "$prompt: " value
      printf '\n'
    else
      read -r -p "$prompt: " value
    fi
  fi

  if [[ -z "$value" ]]; then
    echo "$prompt is required." >&2
    exit 2
  fi

  printf -v "$variable_name" '%s' "$value"
}

BASE_URL="${ONEUPTIME_BASE_URL:-}"
API_KEY="${ONEUPTIME_API_KEY:-}"
PROJECT_ID="${ONEUPTIME_PROJECT_ID:-}"
DASHBOARD_NAME="env2metric 空气质量"

prompt_required_value BASE_URL "OneUptime 地址（例如 https://oneuptime.example.com）"
prompt_required_value API_KEY "OneUptime API Key" true
prompt_required_value PROJECT_ID "OneUptime Project ID"

payload="$(mktemp)"
existing_dashboards="$(mktemp)"
existing_dashboard_ids="$(mktemp)"
existing_dashboard_query="$(mktemp)"
created_dashboard="$(mktemp)"
deleted_dashboard="$(mktemp)"
trap 'rm -f "$payload" "$existing_dashboards" "$existing_dashboard_ids" "$existing_dashboard_query" "$created_dashboard" "$deleted_dashboard"' EXIT

python3 - "$PROJECT_ID" "$DASHBOARD_NAME" >"$payload" <<'PY'
import json
import sys
import uuid

project_id = sys.argv[1]
dashboard_name = sys.argv[2]


def metric_query(metric_name, aggregation="Avg", warning_threshold=None,
                 critical_threshold=None):
    query = {
        "metricQueryData": {
            "filterData": {
                "metricName": metric_name,
                "aggegationType": aggregation,
            },
            "groupBy": None,
        },
    }
    if warning_threshold is not None:
        query["warningThreshold"] = warning_threshold
    if critical_threshold is not None:
        query["criticalThreshold"] = critical_threshold
    return query


def value(title, metric_name, left, top):
    return {
        "_type": "DashboardComponent",
        "componentId": str(uuid.uuid4()),
        "componentType": "Value",
        "topInDashboardUnits": top,
        "leftInDashboardUnits": left,
        "widthInDashboardUnits": 2,
        "heightInDashboardUnits": 1,
        "minWidthInDashboardUnits": 1,
        "minHeightInDashboardUnits": 1,
        "arguments": {
            "title": title,
            "metricQueryConfig": metric_query(metric_name),
        },
    }


def chart(title, metric_name, left, top, warning_threshold=None,
          critical_threshold=None):
    return {
        "_type": "DashboardComponent",
        "componentId": str(uuid.uuid4()),
        "componentType": "Chart",
        "topInDashboardUnits": top,
        "leftInDashboardUnits": left,
        "widthInDashboardUnits": 6,
        "heightInDashboardUnits": 4,
        "minWidthInDashboardUnits": 6,
        "minHeightInDashboardUnits": 3,
        "arguments": {
            "chartTitle": title,
            "chartType": "Line",
            "metricQueryConfig": {
                "metricAliasData": {"metricVariable": "a"},
                **metric_query(metric_name, "Avg", warning_threshold,
                               critical_threshold),
            },
        },
    }


components = [
    value("温度 (°C)", "env2metric.air.temperature", 0, 0),
    value("湿度 (%)", "env2metric.air.humidity", 2, 0),
    value("PM2.5 (µg/m³)", "env2metric.air.pm25", 4, 0),
    value("甲醛 (µg/m³)", "env2metric.air.hcho", 6, 0),
    value("CO₂ (ppm)", "env2metric.air.co2", 8, 0),
    value("VOC (ppm)", "env2metric.air.voc", 10, 0),
    chart("PM2.5 趋势", "env2metric.air.pm25", 0, 1,
          critical_threshold=50),
    chart("温度趋势", "env2metric.air.temperature", 6, 1,
          warning_threshold=25.5),
    chart("湿度趋势", "env2metric.air.humidity", 0, 5,
          warning_threshold=60),
    chart("甲醛趋势", "env2metric.air.hcho", 6, 5,
          warning_threshold=120, critical_threshold=400),
    chart("CO₂ 趋势", "env2metric.air.co2", 0, 9,
          warning_threshold=1000, critical_threshold=2000),
    chart("VOC 趋势", "env2metric.air.voc", 6, 9,
          warning_threshold=0.44, critical_threshold=1.76),
    chart("0.3um 颗粒数趋势", "env2metric.air.particle_0_3um", 0, 13),
]

data = {
    "data": {
        "name": dashboard_name,
        "projectId": project_id,
        "dashboardViewConfig": {
            "_type": "DashboardViewConfig",
            "heightInDashboardUnits": 17,
            "refreshInterval": "1m",
            "components": components,
        },
    },
}
print(json.dumps(data, ensure_ascii=False))
PY

# 仅查找同一项目、同一名称的 Dashboard；重复执行时先删后建，避免留下旧组件配置。
python3 - "$PROJECT_ID" "$DASHBOARD_NAME" >"$existing_dashboard_query" <<'PY'
import json
import sys

print(json.dumps({
    "query": {"projectId": sys.argv[1], "name": sys.argv[2]},
    "select": {"_id": True, "projectId": True, "name": True},
    "sort": {},
    "groupBy": {},
}))
PY

curl --fail-with-body -sS -X POST "${BASE_URL%/}/api/dashboard/get-list?limit=100&skip=0" \
  -H "ApiKey: ${API_KEY}" \
  -H "Content-Type: application/json" \
  --data-binary "@${existing_dashboard_query}" \
  >"$existing_dashboards"

# 兼容 OneUptime 列表接口常见的 data 数组和 data.items 返回格式。
python3 - "$PROJECT_ID" "$DASHBOARD_NAME" "$existing_dashboards" >"$existing_dashboard_ids" <<'PY'
import json
import sys

project_id, dashboard_name, response_path = sys.argv[1:]

with open(response_path, encoding="utf-8") as response_file:
    response = json.load(response_file)

items = response.get("data", []) if isinstance(response, dict) else response
if isinstance(items, dict):
    if "id" in items or "_id" in items:
        items = [items]
    else:
        items = items.get("items", items.get("data", []))

if not isinstance(items, list):
    raise SystemExit("Unexpected OneUptime dashboard list response.")

for item in items:
    item_project_id = item.get("projectId")
    if isinstance(item_project_id, dict):
        item_project_id = item_project_id.get("value")
    if item_project_id != project_id or item.get("name") != dashboard_name:
        continue
    dashboard_id = item.get("id", item.get("_id"))
    if not dashboard_id:
        raise SystemExit("Existing OneUptime dashboard has no id.")
    print(dashboard_id)
PY

while IFS= read -r dashboard_id; do
  [[ -z "$dashboard_id" ]] && continue
  echo "删除已有 Dashboard: ${DASHBOARD_NAME}" >&2
  if ! curl --fail-with-body -sS -X DELETE "${BASE_URL%/}/api/dashboard/${dashboard_id}" \
    -H "ApiKey: ${API_KEY}" \
    >"$deleted_dashboard"; then
    cat "$deleted_dashboard" >&2
    exit 1
  fi
done <"$existing_dashboard_ids"

if ! curl --fail-with-body -sS -X POST "${BASE_URL%/}/api/dashboard" \
  -H "ApiKey: ${API_KEY}" \
  -H "Content-Type: application/json" \
  --data-binary "@${payload}" \
  >"$created_dashboard"; then
  cat "$created_dashboard" >&2
  exit 1
fi

python3 - "$created_dashboard" <<'PY'
import json
import sys

with open(sys.argv[1], encoding="utf-8") as response_file:
    created_dashboard = json.load(response_file)

dashboard_id = created_dashboard.get("_id", created_dashboard.get("id"))
if not dashboard_id:
    raise SystemExit("OneUptime created the Dashboard but returned no id.")

print(f"Dashboard 创建成功: {dashboard_id}")
PY
