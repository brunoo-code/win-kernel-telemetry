# BNQQ Kernel Telemetry

Driver de diagnóstico em **C++ para Windows x64** e cliente **Win32**. O cliente consulta o tempo de atividade do driver e os contadores de consultas aceitas e rejeitadas. O projeto é uma demonstração de comunicação entre modo usuário e modo kernel.

```mermaid
flowchart LR
    C["Cliente Win32"] -->|"DeviceIoControl · consulta"| I["Gerenciador de I/O"]
    I -->|"Buffer do sistema"| D["Driver WDM"]
    D -->|"Snapshot v1 · 40 bytes"| C
    W["WinDbg no host"] -.->|"Breakpoint no dispatch da VM"| D
```

O driver cria um dispositivo de controle com acesso a **SYSTEM e administradores**. Expõe um único IOCTL de leitura com `METHOD_BUFFERED`, rejeita entrada extra e valida o tamanho do buffer. Um spin lock mantém os contadores e a resposta coerentes entre chamadas simultâneas. O contrato contém apenas números de versão, tempo e contadores do próprio driver.

| Campo | Tipo | Conteúdo |
| --- | --- | --- |
| `size`, `abi_version` | 2 × uint32 | Tamanho e versão do contrato |
| `uptime_100ns` | uint64 | Tempo desde que o dispositivo foi inicializado |
| `requests`, `succeeded`, `rejected` | 3 × uint64 | Contadores de IOCTL, incluindo a consulta atual |

## Compilar

Para o cliente e os testes, use CMake, MSVC e o Windows SDK. Em um prompt de desenvolvimento x64:

```powershell
cmake -S . -B build/client -A x64
cmake --build build/client --config Release
ctest --test-dir build/client -C Release --output-on-failure
```

Para o driver, use o SDK 26100 e os arquivos do pacote [Microsoft.Windows.WDK.x64 10.0.26100.6584](https://www.nuget.org/packages/Microsoft.Windows.WDK.x64/10.0.26100.6584). Extraia o pacote e aponte para a subpasta `c` que contém `Include` e `Lib`:

```powershell
.\scripts\build-driver.cmd "C:\WDK\c" "${env:ProgramFiles(x86)}\Windows Kits\10"
```

O build gera `build/driver/BnqqKernelTelemetry.sys` e seu PDB. O artefato local sai sem assinatura.

## Testar e depurar na VM

Em um convidado Windows preparado para drivers de teste, execute o roteiro com o driver assinado e o cliente:

```powershell
.\scripts\test-in-guest.ps1 -Driver C:\Lab\BnqqKernelTelemetry.sys -Client C:\Lab\telemetry-client.exe
```

O autoteste envia uma consulta válida, um IOCTL desconhecido, uma consulta com entrada indevida, uma com saída curta e uma consulta final para conferir os contadores. O [roteiro do WinDbg](docs/windbg.md) mostra como interromper a primeira requisição e comparar a estrutura em kernel com a saída no cliente.

## Validação desta versão

- **Verificado no Windows x64:** compilação e link do driver com MSVC/WDK, compilação do cliente Win32 e teste do contrato de telemetria via CTest.
- **Pendente em VM:** assinatura e carga no convidado, execução do autoteste via `DeviceIoControl`, descarga e captura no WinDbg. O host desta sessão negou a consulta às VMs do Hyper‑V.

Referências: [segurança de objetos de dispositivo](https://learn.microsoft.com/en-us/windows-hardware/drivers/driversecurity/windows-security-model) e [configuração do KDNET](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/setting-up-a-network-debugging-connection-automatically).
