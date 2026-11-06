# Lichii com ajustes de FPS para o Ocarina of Time

Esta branch (`oot-fps`) é o **Lichii build-88** (branch `nx2`, commit `55532f2d8`) com ajustes de desempenho para *The Legend of Zelda: Ocarina of Time* (Switch 2, `0400486024DEC000`). Não é um release oficial e não traz jogo, chaves nem firmware.

## O que muda

Tudo vale só para o Ocarina, e cada item tem uma variável de ambiente para desligar.

| Mudança | Para voltar ao original | Ganho medido |
|---|---|---|
| Copia antes as páginas que a CPU vai ler da memória escrita pela GPU, em vez de esvaziar a GPU a cada leitura | `EDEN_NX2_PREFLUSH=0` | ~+10% |
| Junta as cópias de várias cercas a cada 1 ms (de ~1 GB/s para ~110 MB/s) | `EDEN_NX2_DOWNLOAD_BATCH_US=0` | +3,6% |
| Reenvia os bancos uniformes de 64 KiB a cada 30 quadros por página, em vez de todo quadro | `EDEN_NX2_BANK_REUPLOAD=1` | ~+5% |
| Envia o trabalho adiado para a placa a cada 1 ms (a build-88 usa 0,5 ms) | `EDEN_NX2_FENCE_FLUSH_US=500` | +2,3% |

Os ganhos foram medidos lado a lado num notebook com i9-14900HX e RTX 5060 Laptop, parado no autosave da floresta Kokiri. Juntos, levaram esse ponto de ~33 para ~41 fps. A medição foi feita na base anterior à build-88; em cima da build-88 ainda não houve comparação lado a lado.

Extras:
- `EDEN_NX2_ASYNC_PRESENT=0` ou `1` força a apresentação assíncrona desligada ou ligada.
- Rastreios de diagnóstico no log: `EDEN_NX2_DOWNLOAD_TRACE`, `EDEN_NX2_CHURN_TRACE`, `EDEN_NX2_TRAP_STATS`.

A própria build-88 já traz o mod **Dynamic FPS** embutido, que mantém a velocidade do jogo certa abaixo de 60 fps. Para funcionar direito, deixe o clock da CPU em Normal.

## Compilar no Windows

Você precisa de:
- **Visual Studio 2022** ou **Build Tools 2022**, com a carga "Desenvolvimento para desktop com C++" (traz o compilador, o CMake e o Ninja);
- **Git**;
- **glslangValidator**, que vem no [Vulkan SDK](https://vulkan.lunarg.com/sdk/home#windows). O próprio repositório tem um instalador: rode `tools\windows\install-vulkan-sdk.ps1` num PowerShell aberto como administrador.

O Qt e as outras dependências o CMake baixa sozinho na primeira configuração.

```powershell
git clone --filter=blob:none -b oot-fps https://github.com/marcelog97/Lichii.git
cd Lichii
powershell -ExecutionPolicy Bypass -File tools\windows\compila-oot.ps1
```

O resultado fica em `build\oot\bin\lichii.exe`. A primeira compilação demora bastante; as seguintes são incrementais.

Opções do script:
- `-Jobs 6`: usa menos processos (cada um gasta 1 a 2 GB de RAM).
- `-Glslang C:\caminho\glslangValidator.exe`: indica o glslang se ele não estiver no PATH nem no Vulkan SDK.
- `-Limpar`: apaga a pasta de build antes de compilar.

Para outras plataformas, veja `docs/Build.md`.

## Jogar

1. Abra o `lichii.exe` e aponte a pasta de jogos para onde está o seu `.nup`.
2. Nas configurações do jogo, deixe a precisão da GPU (*GPU Accuracy*) em **Fast**.
3. Para menos engasgos, abra com todos os pipelines do cache montados na abertura:

   ```powershell
   $env:EDEN_NX2_PRELOAD_ALL_PIPELINES = '1'; .\build\oot\bin\lichii.exe
   ```

4. Em notebook com Intel e NVIDIA, vá em Configurações › Sistema › Tela › Gráficos e escolha **Alto desempenho** para o `lichii.exe`. Sem isso, o Vulkan pode usar a Intel.

Testado só em NVIDIA. Há relatos de travamento do Ocarina em placas AMD na build-88, e isso não muda aqui.
