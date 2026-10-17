#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
export COMPOSE_PROJECT_NAME="${COMPOSE_PROJECT_NAME:-vbox-lsilogic-sas-config}"
export PYTHONUNBUFFERED=1

LABEL="vbox-lsilogic-sas-config"
WITNESS="VBOX-LSILOGIC-SAS-CONFIG-WITNESS"

down() {
  echo "== docker compose down -v =="
  docker compose -p "${COMPOSE_PROJECT_NAME}" down -v --remove-orphans || true
}

fail_setup() {
  local reason="$1"
  echo "FAIL ${LABEL} ${reason} ${WITNESS}" | tee poc-last-run.txt
  exit 1
}

trap down EXIT

echo "== docker compose down (clean) =="
down

echo "== docker compose build =="
build_ok=0
for attempt in $(seq 1 5); do
  if docker compose -p "${COMPOSE_PROJECT_NAME}" build; then
    build_ok=1
    break
  fi
  echo "compose-build-retry attempt=${attempt}"
  sleep 8
  down
done
if [[ "${build_ok}" != 1 ]]; then
  fail_setup "compose-build-failed"
fi

echo "== docker compose run --rm gadget =="
set +e
docker compose -p "${COMPOSE_PROJECT_NAME}" run --rm -T --no-deps gadget | tee poc-last-run.txt
rc=${PIPESTATUS[0]}
set -e

if ! tail -n1 poc-last-run.txt 2>/dev/null | grep -qE '^(SUCCESS|FAIL) '; then
  echo "FAIL ${LABEL} poc-exit=${rc} ${WITNESS}" >> poc-last-run.txt
  rc=1
fi

exit "${rc}"
