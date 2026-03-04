#!/bin/bash

# Aborta o script se qualquer comando falhar
set -e

BUILD_DIR="build"

# 1. Verifica se a pasta de build já existe, se não, configura o Meson
if [ ! -d "$BUILD_DIR" ]; then
    echo "--- Configurando o projeto com Meson ---"
    meson setup "$BUILD_DIR" --buildtype=debug
else
    echo "--- Projeto já configurado ---"
fi

# 2. Compila o projeto usando o Ninja (que o Meson usa por baixo)
echo "--- Iniciando a compilação ---"
meson compile -C "$BUILD_DIR"

echo "--- Build concluído com sucesso! ---"
echo "O binário/biblioteca está em: $BUILD_DIR"