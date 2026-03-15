# Web (Emscripten/WASM) — mapeamento

Esta pasta documenta **como a versão web é gerada** e **quais arquivos entram/saem** do build.

## Visão geral

- O build web é feito via **Emscripten** (`emcc`) e gera arquivos estáticos dentro de `docs/`.
- O GitHub Pages publica o conteúdo do repositório e, por convenção, este projeto mantém o “site” em `docs/`.
- A raiz do site (`/`) redireciona para `docs/` via `index.html` (no topo do repositório).

## Entrada do build (source)

- `Makefile` (alvo `web`) — chama `emcc`, configura SDL e empacota assets.
- `web/shell.html` — **template HTML** usado como “shell” pelo Emscripten (`--shell-file`).
- `src/*.c` + `include/*.h` — o jogo (mesmo código do desktop).
- `assets/` — arquivos de áudio e demais recursos empacotados no `.data`.

## Saída do build (artefatos em `docs/`)

Quando você roda `make web`, o Emscripten gera:

- `docs/index.html` — HTML final (shell + injeção do loader do Emscripten).
- `docs/index.js` — loader/runtime JS (cria o `Module`, carrega `.wasm` e `.data`).
- `docs/index.wasm` — o binário WebAssembly do jogo.
- `docs/index.data` — pacote com os arquivos do `assets/` (gerado por `--preload-file assets`).

Observação: **não edite `docs/index.html` manualmente**; edite `web/shell.html` e rode o build novamente.

## Fluxo de carregamento (no browser)

1. O navegador abre `docs/index.html`.
2. `docs/index.html` carrega `docs/index.js`.
3. `docs/index.js`:
   - instancia `docs/index.wasm`;
   - baixa `docs/index.data` e cria os arquivos no FS virtual do Emscripten (`/assets/...`).
4. Após isso, a `main()` C roda normalmente (SDL2 via Emscripten) renderizando no `<canvas>`.

## Deploy (GitHub Pages)

Existem 2 jeitos comuns de publicar:

1. **Pages servindo a raiz do repo** (branch `main` / root):
   - `index.html` (raiz) redireciona para `docs/`;
   - a build web fica em `https://ens-emilio.github.io/zezyi-C/docs/`.

2. **Pages servindo `docs/` diretamente** (branch `main` / folder `/docs`):
   - o jogo fica na raiz do site, e `docs/` vira “/” no Pages.

Se a página abrir “só texto”, quase sempre é porque o Pages está servindo um HTML/README (Jekyll) e não o `docs/index.html`.

## Como gerar localmente

Pré-requisito: ter o Emscripten instalado e com ambiente carregado (ex.: `source <path>/emsdk_env.sh`).

- Build: `make web`
- Teste local: sirva a pasta `docs/` com um servidor HTTP (não use `file://`).
  - Ex.: `python3 -m http.server 8000 --directory docs` e abra `http://localhost:8000/`.

## Troubleshooting rápido

- Tela em branco / só “Loading…”: verifique no DevTools → Network se `index.js`, `index.wasm` e `index.data` retornam 200.
- Erro 404 para `index.js/.wasm/.data`: você está abrindo a URL errada (ex.: `/` sem redirecionar, ou Pages apontando para a pasta errada).
- Mudou `web/shell.html` e não refletiu no site: faltou rodar `make web` e commitar/push dos arquivos em `docs/`.
