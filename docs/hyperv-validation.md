# Validação no Hyper‑V

Em 11 de novembro de 2026, o driver foi carregado e testado em uma VM Windows Server 2025 x64 (família de build 26100). A VM usou uma assinatura de teste e uma porta serial virtual para conectar ao KD 10.0.29680.1004, distribuído no pacote WinDbg. O depurador carregou os símbolos privados do PDB e executou o breakpoint em `KernelTelemetryDeviceControl` para as cinco chamadas do cliente.

| Chamada | IOCTL | Entrada (bytes) | Saída solicitada (bytes) | Resultado |
| ---: | --- | ---: | ---: | --- |
| 1 | `0x80006000` | 0 | 40 | Consulta aceita |
| 2 | `0x80006004` | 0 | 40 | IOCTL desconhecido rejeitado |
| 3 | `0x80006000` | 1 | 40 | Entrada indevida rejeitada |
| 4 | `0x80006000` | 0 | 1 | Buffer de saída curto rejeitado |
| 5 | `0x80006000` | 0 | 40 | Consulta aceita e contadores conferidos |

## Depurador

Breakpoint usado para registrar cada requisição sem parar o autoteste:

```text
bu BnqqKernelTelemetry!KernelTelemetryDeviceControl ".echo BNQQ_DRIVER_DISPATCH; lmvm BnqqKernelTelemetry; !irp @rdx; gc"
```

Trecho da quarta chamada. Endereços e caminhos locais foram omitidos.

```text
BNQQ_DRIVER_DISPATCH
BnqqKernelTelemetry   (private pdb symbols)
>[IRP_MJ_DEVICE_CONTROL(e), N/A(0)]
    \Driver\BnqqKernelTelemetry
    Args: 00000001 00000000 0x80006000 00000000
```

## Cliente Win32 e descarga

```text
PASS: unsupported IOCTL, unexpected input, short output and valid query
ABI version: 1
Driver uptime (ms): 65
Requests: 5
Succeeded: 2
Rejected: 3
```

O script registrou o driver como `RUNNING` antes do teste e `STOPPED` depois. A remoção do serviço retornou `[SC] DeleteService SUCCESS`.

A primeira execução revelou que o cliente dependia do runtime MSVC, ausente no convidado. O cliente foi recompilado com link estático do runtime e a segunda execução passou. O teste cobre carga, consulta, rejeição dos três formatos inválidos e descarga. O breakpoint captura a requisição na entrada. Os contadores foram verificados na resposta do cliente.

## Teste de carga concorrente

Em 11 de novembro de 2026, a VM **BNQQ-Driver-Lab** no Hyper‑V executou o cliente em Windows Server 2025 Datacenter Evaluation x64 (build 26100, dois processadores virtuais). O depurador foi retomado com os breakpoints limpos para que cada chamada pudesse seguir sem interrupção.

Em PowerShell elevado no convidado, com o driver assinado para a VM:

```powershell
.\scripts\test-in-guest.ps1 -Driver C:\Lab\BnqqKernelTelemetry.sys -Client C:\Lab\telemetry-client.exe -Mode Stress
```

Cada uma das oito conexões de carga fez 250 consultas. O cliente verificou a sequência de contadores em cada conexão. Uma conexão de controle fez uma leitura antes e outra depois, totalizando **2.002 requisições**.

```text
PASS: 8 independent handles, 2000 worker queries
ABI version: 1
Driver uptime (ms): 325
Requests: 2002
Succeeded: 2002
Rejected: 0
```

O valor de 325 ms é o tempo de atividade do driver no instante da leitura final. Esta execução verificou a consistência dos contadores sob concorrência. O serviço terminou em `STOPPED`, a remoção retornou `DeleteService SUCCESS` e uma consulta posterior confirmou que ele não estava mais registrado.

## Validação do painel

O painel WinForms foi aberto na sessão de área de trabalho da VM. O modo de teste acionou os botões **Consultar driver** e **Rodar teste de carga** e chamou a mesma rotina de gravação usada pelo botão **Exportar relatório**. O arquivo exportado registrou:

| Operação | Requisições | Aceitas | Rejeitadas | Código de saída |
| --- | ---: | ---: | ---: | ---: |
| Consultar driver | 1 | 1 | 0 | 0 |
| Rodar teste de carga | 2.002 | 2.002 | 0 | 0 |

O atalho foi instalado na área de trabalho, e a política do wallpaper foi conferida na conta do laboratório. Após verificar que o serviço do driver havia sido removido, o Hyper‑V confirmou o checkpoint de produção **BNQQ Driver Lab - painel validado**.
