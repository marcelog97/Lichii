# Lichii com ajustes de FPS e menos engasgos para o Ocarina of Time

Esta branch (`oot-fps`) é o **Lichii oficial** (branch `nx2`, commit `588bfa611`, posterior à build-88) com ajustes de desempenho para *The Legend of Zelda: Ocarina of Time* (Switch 2, `0400486024DEC000`). Não é um release oficial e não traz jogo, chaves nem firmware.

## O que muda

Tudo vale só para o Ocarina, salvo onde está indicado. Cada item tem uma variável de ambiente para voltar ao comportamento original.

### FPS

| Mudança | Para voltar ao original | Ganho medido |
|---|---|---|
| Copia antes as páginas que a CPU vai ler da memória escrita pela GPU, em vez de esvaziar a GPU a cada leitura | `EDEN_NX2_PREFLUSH=0` | ~+10% |
| Junta as cópias de várias cercas a cada 1 ms (de ~1 GB/s para ~110 MB/s) | `EDEN_NX2_DOWNLOAD_BATCH_US=0` | +3,6% |
| Reenvia os bancos uniformes de 64 KiB a cada 30 quadros por página, em vez de todo quadro | `EDEN_NX2_BANK_REUPLOAD=1` | ~+5% |
| Envia o trabalho adiado para a placa a cada 1 ms (o oficial usa 0,5 ms) | `EDEN_NX2_FENCE_FLUSH_US=500` | +2,3% |

Juntos, esses ajustes levaram o autosave da floresta Kokiri de ~33 para ~41 fps, num notebook com i9-14900HX e RTX 5060 Laptop.

### Engasgos

| Mudança | Para voltar ao original | Efeito medido |
|---|---|---|
| Nunca descarta pipelines e monta todo o cache de pipelines na abertura | `EDEN_NX2_PIPELINE_EVICT_FRAMES=9000` e `EDEN_NX2_PRELOAD_ALL_PIPELINES=0` | Acabou com as rajadas de remontagem: eram 48 dos 51 engasgos de uma sessão de 25 min |
| Só marca memória como "fria" (tira páginas da RAM do processo) quando há menos de 2 GB disponíveis (vale para todos os jogos) | `EDEN_NX2_HINT_COLD=1` | Evita quadros de até 0,5 s com dezenas de milhares de faltas de página quando sobra RAM |

### Não travar na primeira vez que algo aparece (ligado por padrão no Ocarina)

O pulo vem do PR #224 do projeto oficial (`EDEN_NX2_ASYNC_DISCARD`), com quatro acréscimos daqui:
- vem ligado por padrão no Ocarina, sem precisar da opção de shaders assíncronos da interface; `EDEN_NX2_ASYNC_DISCARD=0` desliga;
- dá até 12 ms para o pipeline terminar antes de pular (`EDEN_NX2_ASYNC_GRACE_MS`), então o que já está no cache da placa nunca é pulado;
- pula só desenhos 3D com teste de profundidade, porque pular desenhos de interface deixava o minimapa vazio;
- nunca pula pipelines que gravam em memória nem pipelines de malha.

Com ele, quando a placa compila algo pela primeira vez (0,1 a 2,6 s), o objeto some por alguns quadros em vez de travar a imagem. Na primeira visita a uma área, com os caches vazios, os quadros acima de 100 ms caíram de 78,6 para 36,8 a cada 10 minutos, e nenhuma captura mostrou defeito. O log da abertura confirma que ele está ligado: "Native draws skip pipelines still building after 12 ms".

### Outros

- O log registra o que cada quadro acima de 100 ms esperou: `nx2:vi2 long frame` (com as faltas de página), `Nx2Perf hitch: draws waited` (compilação de pipeline) e `Nx2Perf hitch: texture collector took` (limpeza do cache de texturas).
- Chaves de medição: `EDEN_NX2_PIPELINE_NO_OPT`, `EDEN_NX2_EXIT_AFTER_PRELOAD`, `EDEN_NX2_EXIT_AFTER_S`, `EDEN_NX2_DOWNLOAD_TRACE`, `EDEN_NX2_CHURN_TRACE`, `EDEN_NX2_TRAP_STATS`, `EDEN_NX2_ASYNC_PRESENT=0/1`.

O projeto oficial já traz o mod **Dynamic FPS** embutido, que mantém a velocidade do jogo certa abaixo de 60 fps. Para funcionar direito, deixe o clock da CPU em Normal.

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

Para atualizar depois: `git pull` e rode o script de novo.

O resultado fica em `build\oot\bin\lichii.exe`. A primeira compilação demora bastante; as seguintes são incrementais.

Opções do script:
- `-Jobs 6`: usa menos processos (cada um gasta 1 a 2 GB de RAM).
- `-Glslang C:\caminho\glslangValidator.exe`: indica o glslang se ele não estiver no PATH, no Vulkan SDK nem em `C:\VulkanSDK`.
- `-Limpar`: apaga a pasta de build antes de compilar.

Para outras plataformas, veja `docs/Build.md`.

## Jogar

1. Abra o `lichii.exe` e aponte a pasta de jogos para onde está o seu `.nup`.
2. Nas configurações do jogo, deixe a precisão da GPU (*GPU Accuracy*) em **Fast**.
3. O pulo de compilação já vem ligado. Para desligá-lo e sempre esperar a compilação:

   ```powershell
   $env:EDEN_NX2_ASYNC_DISCARD = '0'; .\build\oot\bin\lichii.exe
   ```

4. Em notebook com Intel e NVIDIA, vá em Configurações › Sistema › Tela › Gráficos e escolha **Alto desempenho** para o `lichii.exe`. Sem isso, o Vulkan pode usar a Intel.

A abertura monta todos os pipelines que já estão no cache. Depois de atualizar o emulador, a primeira abertura pode demorar mais, porque a placa recompila o que mudou. Com pouca RAM livre (menos de ~4 GB com o jogo aberto), feche programas pesados: o Windows passa a paginar e aparecem engasgos.

Testado só em NVIDIA. Há relatos de travamento do Ocarina em placas AMD no projeto oficial.
