// SPDX-License-Identifier: MIT
// Read-only libusb enumeration. Uses the DLL beside the selected programmer;
// no driver installation, device opening, control transfers or flash operations.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Runtime.InteropServices;

public sealed class Uc3DfuUsbLocation {
    public int Bus;
    public int Address;
}

public static class Uc3DfuUsbLocator {
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
    [DllImport("kernel32.dll", CharSet = CharSet.Ansi, ExactSpelling = true)]
    private static extern IntPtr GetProcAddress(IntPtr module, string name);
    [DllImport("kernel32.dll")]
    [return: MarshalAs(UnmanagedType.Bool)]
    private static extern bool FreeLibrary(IntPtr module);

    [StructLayout(LayoutKind.Sequential, Pack = 1)]
    private struct Descriptor {
        public byte Length, Type;
        public ushort UsbVersion;
        public byte Class, Subclass, Protocol, PacketSize;
        public ushort Vendor, Product, DeviceVersion;
        public byte Manufacturer, ProductString, Serial, Configurations;
    }
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate int Init(out IntPtr context);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate void Exit(IntPtr context);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate IntPtr GetList(IntPtr context, out IntPtr list);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate void FreeList(IntPtr list, int unref);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate int GetDescriptor(IntPtr device, out Descriptor descriptor);
    [UnmanagedFunctionPointer(CallingConvention.Winapi)]
    private delegate byte GetNumber(IntPtr device);

    private static T Function<T>(IntPtr module, string name) where T : class {
        IntPtr address = GetProcAddress(module, name);
        if (address == IntPtr.Zero) throw new InvalidOperationException("Missing libusb function: " + name);
        return Marshal.GetDelegateForFunctionPointer(address, typeof(T)) as T;
    }

    public static Uc3DfuUsbLocation[] Find(string library) {
        if (!Environment.Is64BitProcess) throw new InvalidOperationException("Use 64-bit PowerShell with dfu-programmer x64.");
        // Restrict dependency lookup to this DLL's directory and Windows defaults.
        IntPtr module = LoadLibraryExW(library, IntPtr.Zero, 0x1100);
        if (module == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "Cannot load programmer's libusb DLL.");
        try {
            Init init = Function<Init>(module, "libusb_init");
            Exit exit = Function<Exit>(module, "libusb_exit");
            GetList getList = Function<GetList>(module, "libusb_get_device_list");
            FreeList freeList = Function<FreeList>(module, "libusb_free_device_list");
            GetDescriptor getDescriptor = Function<GetDescriptor>(module, "libusb_get_device_descriptor");
            GetNumber getBus = Function<GetNumber>(module, "libusb_get_bus_number");
            GetNumber getAddress = Function<GetNumber>(module, "libusb_get_device_address");
            IntPtr context;
            int result = init(out context);
            if (result < 0) throw new InvalidOperationException("libusb_init failed: " + result);
            try {
                IntPtr list;
                long count = getList(context, out list).ToInt64();
                if (count < 0) throw new InvalidOperationException("libusb enumeration failed: " + count);
                try {
                    if (count > 65536 || (count > 0 && list == IntPtr.Zero)) throw new InvalidOperationException("Invalid libusb device list.");
                    List<Uc3DfuUsbLocation> matches = new List<Uc3DfuUsbLocation>();
                    for (int i = 0; i < count; ++i) {
                        IntPtr device = Marshal.ReadIntPtr(list, i * IntPtr.Size);
                        if (device == IntPtr.Zero) throw new InvalidOperationException("Incomplete libusb device list.");
                        Descriptor descriptor;
                        if (getDescriptor(device, out descriptor) < 0) throw new InvalidOperationException("Cannot inspect a USB device descriptor.");
                        if (descriptor.Vendor == 0x03eb && descriptor.Product == 0x2ff6) {
                            matches.Add(new Uc3DfuUsbLocation { Bus = getBus(device), Address = getAddress(device) });
                        }
                    }
                    return matches.ToArray();
                } finally { if (list != IntPtr.Zero) freeList(list, 1); }
            } finally { exit(context); }
        } finally { FreeLibrary(module); }
    }
}
