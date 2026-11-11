# Demo no WinDbg

Este roteiro está pronto para execução. Ainda não há captura de uma sessão de kernel para esta versão.

## Preparação

1. Use uma VM Windows x64 descartável, com snapshot e provisionada para testes de drivers. Assine o `.sys` de acordo com o ambiente do convidado. A [documentação de assinatura de teste da Microsoft](https://learn.microsoft.com/en-us/windows-hardware/drivers/install/test-signing) explica as opções.
2. Copie o driver assinado e o cliente para o convidado. Mantenha o PDB produzido pelo mesmo build em uma pasta de símbolos acessível ao WinDbg.
3. Conecte o WinDbg do host ao convidado usando [KDNET](https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/setting-up-a-network-debugging-connection-automatically). A configuração inicial de depuração pode exigir reinício **do convidado**.

## Capturar uma consulta

Com o convidado pausado pelo WinDbg, aponte para a pasta do PDB e prepare o ponto de interrupção adiado:

```text
.symfix
.sympath+ C:\Symbols\BnqqKernelTelemetry
bu BnqqKernelTelemetry!KernelTelemetryDeviceControl
bl
g
```

Em um PowerShell elevado **dentro da VM**, rode `scripts/test-in-guest.ps1` com os caminhos do driver e do cliente. A primeira consulta do cliente deve atingir o ponto de interrupção. No WinDbg:

```text
lmvm BnqqKernelTelemetry
k
!irp @rdx
p
dv /t
```

No ponto de entrada x64, `rdx` contém o IRP. Com `p`, avance até depois da chamada a `KeReleaseSpinLock`; use `dv /t` para localizar `snapshot` e `dx snapshot` para ver os campos. Compare os valores com a saída do cliente. Use `bl`, desative o ponto de interrupção pelo número exibido (`bd <id>`) e execute `g` para permitir que o autoteste finalize.

Em um carregamento novo sem outros clientes, a saída esperada é **2 consultas aceitas e 3 rejeitadas**, total de 5. O script descarrega o driver e remove o serviço criado para o teste. Registre a saída do cliente e o trecho do WinDbg com `lmvm` e `dx snapshot` para documentar a execução.
