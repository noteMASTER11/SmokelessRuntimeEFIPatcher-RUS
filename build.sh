#!/usr/bin/env bash

set -euo pipefail

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "Этот скрипт предназначен для macOS. Текущая система: $(uname -s)" >&2
  exit 1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEPS_DIR="${ROOT_DIR}/.deps"
EDK2_DIR="${DEPS_DIR}/edk2"
DEFAULT_LLVM_PREFIX="$(brew --prefix llvm 2>/dev/null || true)"

TARGET_ARCH="${TARGET_ARCH:-X64}"
BUILD_MODE="${BUILD_MODE:-RELEASE}"
TOOLCHAIN_TAG="${TOOLCHAIN_TAG:-CLANGPDB}"

function ensure_command() {
  local cmd="$1"
  local hint="$2"
  if ! command -v "${cmd}" >/dev/null 2>&1; then
    echo "Не найдено '${cmd}'. Установите его (${hint}) и повторите попытку." >&2
    exit 1
  fi
}

ensure_command brew "brew install <package>"
ensure_command git "brew install git"
ensure_command python3 "brew install python"
export PYTHON_COMMAND="$(command -v python3)"
ensure_command nasm "brew install nasm"

if [[ -n "${DEFAULT_LLVM_PREFIX}" ]]; then
  export PATH="${DEFAULT_LLVM_PREFIX}/bin:${PATH}"
fi
ensure_command clang "brew install llvm"

mkdir -p "${DEPS_DIR}"

if [[ ! -d "${EDK2_DIR}" ]]; then
  echo "Клонирую edk2..."
  git clone --depth 1 https://github.com/tianocore/edk2.git "${EDK2_DIR}"
fi

pushd "${EDK2_DIR}" >/dev/null

if [[ -f ".gitmodules" ]]; then
  echo "Обновляю подмодули edk2..."
  git submodule update --init --recursive
fi

if [[ ! -d "BaseTools" || ! -x "BaseTools/BinWrappers/PosixLike/build" ]]; then
  echo "Готовлю BaseTools..."
  make -C BaseTools
fi

export WORKSPACE="${EDK2_DIR}"
export PACKAGES_PATH="${WORKSPACE}"
export EDK_TOOLS_PATH="${WORKSPACE}/BaseTools"

ln -snf "${ROOT_DIR}" "${WORKSPACE}/SREPPkg"

mkdir -p "${WORKSPACE}/Conf"
export CONF_PATH="${WORKSPACE}/Conf"
source edksetup.sh >/dev/null

cat > "${WORKSPACE}/Conf/target.txt" <<EOF
ACTIVE_PLATFORM  = SREPPkg/SmokelessRuntimeEFIPatcher.dsc
TARGET           = ${BUILD_MODE}
TARGET_ARCH      = ${TARGET_ARCH}
TOOL_CHAIN_TAG   = ${TOOLCHAIN_TAG}
BUILD_RULE_CONF  = Conf/build_rule.txt
MAX_CONCURRENT_THREAD_NUMBER = ${MAX_CONCURRENT_THREAD_NUMBER:-0}
EOF

echo "Запускаю сборку: build -a ${TARGET_ARCH} -b ${BUILD_MODE} -t ${TOOLCHAIN_TAG}"
build -a "${TARGET_ARCH}" -b "${BUILD_MODE}" -t "${TOOLCHAIN_TAG}" -p SREPPkg/SmokelessRuntimeEFIPatcher.dsc

OUTPUT_DIR="${WORKSPACE}/Build/SmokelessRuntimeEFIPatcher/${BUILD_MODE}_${TOOLCHAIN_TAG}/SREPPkg/SmokelessRuntimeEFIPatcher/SmokelessRuntimeEFIPatcher/OUTPUT"

popd >/dev/null

if [[ -f "${OUTPUT_DIR}/SmokelessRuntimeEFIPatcher.efi" ]]; then
  echo "Сборка завершена. Файл: ${OUTPUT_DIR}/SmokelessRuntimeEFIPatcher.efi"
else
  echo "Сборка завершилась, но бинарник не найден. Проверьте логи." >&2
  exit 1
fi

