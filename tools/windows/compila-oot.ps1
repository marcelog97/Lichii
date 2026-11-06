<#
  compila-oot.ps1 - compila esta branch com MSVC, com as mesmas opcoes do CI do Lichii
  (job windows-x86_64 de .github/workflows/release-binaries.yml).

  Uso, na raiz do repositorio:
    powershell -ExecutionPolicy Bypass -File tools\windows\compila-oot.ps1 [-Jobs 6] [-Glslang C:\...\glslangValidator.exe] [-Limpar]

  Requisitos: Visual Studio 2022 ou Build Tools 2022 com "Desenvolvimento para desktop com C++"
  (cl, CMake e Ninja vem junto), Git e o glslangValidator (Vulkan SDK). O Qt e as demais
  dependencias o CMake baixa sozinho.

  Saida: build\oot\bin\lichii.exe
#>
param(
    [int]$Jobs = [Math]::Max(2, [Math]::Min([Environment]::ProcessorCount, 12)),
    [string]$Glslang = '',
    [switch]$Limpar
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$build = Join-Path $repo 'build\oot'

# O CMake procura glslangValidator; desde a 14 o glslang oficial tambem vem como glslang.exe.
if (-not $Glslang) {
    $achado = Get-Command glslangValidator.exe, glslang.exe -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($achado) {
        $Glslang = $achado.Source
    } elseif ($env:VULKAN_SDK) {
        foreach ($nome in 'glslangValidator.exe', 'glslang.exe') {
            $candidato = Join-Path $env:VULKAN_SDK "Bin\$nome"
            if (Test-Path $candidato) { $Glslang = $candidato; break }
        }
    }
    if (-not $Glslang -and (Test-Path 'C:\VulkanSDK')) {
        # Onde tools\windows\install-vulkan-sdk.ps1 instala; a versao mais nova primeiro.
        $achado = Get-ChildItem 'C:\VulkanSDK' -Recurse -File -ErrorAction SilentlyContinue |
            Where-Object { $_.Name -in 'glslangValidator.exe', 'glslang.exe' } |
            Sort-Object FullName -Descending | Select-Object -First 1
        if ($achado) { $Glslang = $achado.FullName }
    }
}
if (-not $Glslang -or -not (Test-Path $Glslang)) {
    throw 'glslangValidator nao encontrado: instale o Vulkan SDK ou passe -Glslang C:\caminho\glslangValidator.exe'
}

& (Join-Path $repo 'tools\windows\load-msvc-env.ps1')
foreach ($t in 'cl', 'cmake', 'ninja') {
    if (-not (Get-Command $t -ErrorAction SilentlyContinue)) {
        throw "$t nao esta no PATH: instale o Visual Studio 2022 com C++ (Desenvolvimento para desktop com C++)"
    }
}
if ($Limpar -and (Test-Path $build)) { Remove-Item -LiteralPath $build -Recurse -Force }

$sha = (git -C $repo rev-parse --short=9 HEAD).Trim()
"Compilando oot-$sha com $Jobs processos (glslang: $Glslang)"
cmake -S $repo -B $build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DYUZU_TESTS=OFF -DYUZU_CMD=ON `
    -DYUZU_ROOM=OFF -DYUZU_ROOM_STANDALONE=OFF -DYUZU_USE_BUNDLED_SDL3=ON `
    -DENABLE_UPDATE_CHECKER=OFF "-DLICHII_BUILD_TAG=oot-$sha" -DLICHII_BUILD_PLATFORM=windows-x86_64 `
    -DUSE_DISCORD_PRESENCE=OFF "-DGLSLANGVALIDATOR=$($Glslang -replace '\\', '/')"
if ($LASTEXITCODE) { throw "cmake (configuracao) falhou: $LASTEXITCODE" }

$t0 = Get-Date
cmake --build $build --target yuzu yuzu-cmd --parallel $Jobs
if ($LASTEXITCODE) { throw "compilacao falhou: $LASTEXITCODE" }
'Pronto em {0:N1} min: {1}' -f ((Get-Date) - $t0).TotalMinutes, (Join-Path $build 'bin\lichii.exe')
