#include <ntddk.h>
#include <wdmsec.h>

#include "../include/telemetry.hpp"

static_assert(telemetry::query == CTL_CODE(telemetry::device_type, telemetry::function_query,
                                         METHOD_BUFFERED, FILE_READ_ACCESS));

namespace {
// Device-specific class GUID. The named device is accessible to SYSTEM and Administrators.
const GUID device_class{0xc4d0e6b0, 0xf675, 0x4ce3, {0x8a, 0x13, 0xa0, 0x2c, 0xf1, 0xf8, 0x63, 0x93}};
UNICODE_STRING link_name = RTL_CONSTANT_STRING(L"\\DosDevices\\BnqqKernelTelemetry");

struct DeviceState {
    KSPIN_LOCK lock;
    ULONGLONG started;
    telemetry::Counters counters;
};

NTSTATUS complete(PIRP irp, NTSTATUS status, ULONG_PTR bytes = 0) {
    irp->IoStatus.Status = status;
    irp->IoStatus.Information = bytes;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS open_close(PDEVICE_OBJECT, PIRP irp) {
    const auto stack = IoGetCurrentIrpStackLocation(irp);
    if (stack->MajorFunction == IRP_MJ_CREATE && stack->FileObject->FileName.Length != 0) {
        return complete(irp, STATUS_OBJECT_NAME_NOT_FOUND);
    }
    return complete(irp, STATUS_SUCCESS);
}

void unload(PDRIVER_OBJECT driver) {
    IoDeleteSymbolicLink(&link_name);
    IoDeleteDevice(driver->DeviceObject);
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "BnqqKernelTelemetry: unloaded\n");
}
}

extern "C" DRIVER_DISPATCH KernelTelemetryDeviceControl;
extern "C" NTSTATUS KernelTelemetryDeviceControl(PDEVICE_OBJECT device, PIRP irp) {
    const auto stack = IoGetCurrentIrpStackLocation(irp);
    const auto& ioctl = stack->Parameters.DeviceIoControl;
    auto* state = static_cast<DeviceState*>(device->DeviceExtension);
    telemetry::Snapshot snapshot{};
    KIRQL old_irql;
    KeAcquireSpinLock(&state->lock, &old_irql);
    const auto result = state->counters.dispatch(ioctl.IoControlCode, ioctl.InputBufferLength,
        ioctl.OutputBufferLength, irp->AssociatedIrp.SystemBuffer ? &snapshot : nullptr,
        KeQueryInterruptTime() - state->started);
    KeReleaseSpinLock(&state->lock, old_irql);

    switch (result) {
    case telemetry::Result::success:
        RtlCopyMemory(irp->AssociatedIrp.SystemBuffer, &snapshot, sizeof(snapshot));
        return complete(irp, STATUS_SUCCESS, sizeof(snapshot));
    case telemetry::Result::unsupported:
        return complete(irp, STATUS_INVALID_DEVICE_REQUEST);
    case telemetry::Result::invalid_input:
        return complete(irp, STATUS_INVALID_PARAMETER);
    case telemetry::Result::small_buffer:
        return complete(irp, STATUS_BUFFER_TOO_SMALL);
    }
    return complete(irp, STATUS_INTERNAL_ERROR);
}

extern "C" DRIVER_INITIALIZE DriverEntry;
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING) {
    UNICODE_STRING name = RTL_CONSTANT_STRING(L"\\Device\\BnqqKernelTelemetry");
    UNICODE_STRING access = RTL_CONSTANT_STRING(L"D:P(A;;GA;;;SY)(A;;GA;;;BA)");
    PDEVICE_OBJECT device = nullptr;
    auto status = IoCreateDeviceSecure(driver, sizeof(DeviceState), &name,
        telemetry::device_type, FILE_DEVICE_SECURE_OPEN, FALSE, &access, &device_class, &device);
    if (!NT_SUCCESS(status)) return status;

    auto* state = static_cast<DeviceState*>(device->DeviceExtension);
    KeInitializeSpinLock(&state->lock);
    state->started = KeQueryInterruptTime();
    state->counters = {};
    driver->MajorFunction[IRP_MJ_CREATE] = open_close;
    driver->MajorFunction[IRP_MJ_CLEANUP] = open_close;
    driver->MajorFunction[IRP_MJ_CLOSE] = open_close;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = KernelTelemetryDeviceControl;
    driver->DriverUnload = unload;
    status = IoCreateSymbolicLink(&link_name, &name);
    if (!NT_SUCCESS(status)) {
        IoDeleteDevice(device);
        return status;
    }
    device->Flags &= ~DO_DEVICE_INITIALIZING;
    DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "BnqqKernelTelemetry: loaded, ABI v%u\n",
               telemetry::version);
    return STATUS_SUCCESS;
}
