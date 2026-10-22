# SPDX-License-Identifier: GPL-2.0-or-later
"""Windows device access for util/evision_flash.py and util/evision_dump.py. On Windows the tools load this file in
place of their sysfs and hidraw functions; the functions here have the same names and return the same values.
SetupAPI, cfgmgr32 and hid.dll through ctypes; Python standard library only. Any user can open vendor and consumer
HID collections: no driver and no administrator rights are needed.

Windows creates one HID device per top-level collection. On USB interface 1 the tools use the vendor collection of
the stock firmware (usage page 0xFF1C; the interface also has keyboard, system control, consumer control and mouse
collections), the only collection of the OEM bootloader (consumer page, 64-byte feature report without a
report ID) and QMK's raw HID collection (usage page 0xFF60, 32-byte reports without a report ID).

A USB device is identified by its location path (e.g. PCIROOT(0)#PCI(1400)#USBROOT(0)#USB(2)#USB(3)). The path is
the same when a board enumerates again with another USB ID at the same port, as the sysfs name of the port is on
Linux.
"""
import re, time, errno, collections

try:
    import ctypes
    from ctypes import wintypes as W
    CTYPES_ERROR = None
except ImportError as e:                    # e.g. Smart App Control blocks the libffi DLL of an unsigned Python
    CTYPES_ERROR = e

# The interface-1 collection that the tools use, by USB ID. Other devices: interface 1 if it has one collection.
IFACE1_PAGE = {(0x320f, 0x5000): 0xff1c, (0x0c45, 0x7687): 0x000c}
IFACE1_PAGE.update({(0x5244, pid): 0xff60 for pid in (0x0580, 0x0552, 0x0551, 0x0582, 0x0589)})   # QMK raw HID
CONTROL_TIMEOUT = 5.0                       # seconds; the Linux USB core's timeout for hidraw's control transfers

GENERIC_READ_WRITE = 0xC0000000
FILE_SHARE_READ_WRITE = 3
OPEN_EXISTING = 3
FILE_FLAG_OVERLAPPED = 0x40000000
DIGCF_PRESENT, DIGCF_DEVICEINTERFACE = 0x02, 0x10
CM_DRP_LOCATION_INFORMATION, CM_DRP_LOCATION_PATHS = 0x0E, 0x24
IOCTL_HID_SET_FEATURE, IOCTL_HID_GET_FEATURE = 0xB0191, 0xB0192
ERROR_IO_PENDING = 997
WAIT_OBJECT_0 = 0

UsbDevice = collections.namedtuple("UsbDevice", "location instance vid pid bcd serial hidpath")

if CTYPES_ERROR is None:
    class GUID(ctypes.Structure):
        _fields_ = [("Data1", W.DWORD), ("Data2", W.WORD), ("Data3", W.WORD), ("Data4", ctypes.c_ubyte * 8)]

    class SP_DEVICE_INTERFACE_DATA(ctypes.Structure):
        _fields_ = [("cbSize", W.DWORD), ("InterfaceClassGuid", GUID), ("Flags", W.DWORD),
                    ("Reserved", ctypes.c_size_t)]

    class SP_DEVINFO_DATA(ctypes.Structure):
        _fields_ = [("cbSize", W.DWORD), ("ClassGuid", GUID), ("DevInst", W.DWORD), ("Reserved", ctypes.c_size_t)]

    class HIDD_ATTRIBUTES(ctypes.Structure):
        _fields_ = [("Size", W.ULONG), ("VendorID", W.USHORT), ("ProductID", W.USHORT),
                    ("VersionNumber", W.USHORT)]

    class HIDP_CAPS(ctypes.Structure):
        _fields_ = [("Usage", W.USHORT), ("UsagePage", W.USHORT), ("InputReportByteLength", W.USHORT),
                    ("OutputReportByteLength", W.USHORT), ("FeatureReportByteLength", W.USHORT),
                    ("Reserved", W.USHORT * 17), ("Counts", W.USHORT * 10)]

    class OVERLAPPED(ctypes.Structure):
        _fields_ = [("Internal", ctypes.c_size_t), ("InternalHigh", ctypes.c_size_t), ("Offset", W.DWORD),
                    ("OffsetHigh", W.DWORD), ("hEvent", W.HANDLE)]

    def _fn(dll, name, restype, *argtypes):
        f = getattr(dll, name)
        f.restype, f.argtypes = restype, argtypes
        return f

    _hid, _setupapi, _cfgmgr, _kernel = (ctypes.WinDLL(n, use_last_error=True)
                                         for n in ("hid", "setupapi", "cfgmgr32", "kernel32"))
    _P, _VP = ctypes.POINTER, ctypes.c_void_p
    HidD_GetHidGuid = _fn(_hid, "HidD_GetHidGuid", None, _P(GUID))
    HidD_GetAttributes = _fn(_hid, "HidD_GetAttributes", W.BOOLEAN, W.HANDLE, _P(HIDD_ATTRIBUTES))
    HidD_GetPreparsedData = _fn(_hid, "HidD_GetPreparsedData", W.BOOLEAN, W.HANDLE, _P(_VP))
    HidD_FreePreparsedData = _fn(_hid, "HidD_FreePreparsedData", W.BOOLEAN, _VP)
    HidP_GetCaps = _fn(_hid, "HidP_GetCaps", W.LONG, _VP, _P(HIDP_CAPS))
    HidD_GetManufacturerString = _fn(_hid, "HidD_GetManufacturerString", W.BOOLEAN, W.HANDLE, _VP, W.ULONG)
    HidD_GetProductString = _fn(_hid, "HidD_GetProductString", W.BOOLEAN, W.HANDLE, _VP, W.ULONG)
    SetupDiGetClassDevsW = _fn(_setupapi, "SetupDiGetClassDevsW", W.HANDLE, _P(GUID), W.LPCWSTR, W.HWND, W.DWORD)
    SetupDiEnumDeviceInterfaces = _fn(_setupapi, "SetupDiEnumDeviceInterfaces", W.BOOL, W.HANDLE, _VP, _P(GUID),
                                      W.DWORD, _P(SP_DEVICE_INTERFACE_DATA))
    SetupDiGetDeviceInterfaceDetailW = _fn(_setupapi, "SetupDiGetDeviceInterfaceDetailW", W.BOOL, W.HANDLE,
                                           _P(SP_DEVICE_INTERFACE_DATA), _VP, W.DWORD, _P(W.DWORD),
                                           _P(SP_DEVINFO_DATA))
    SetupDiDestroyDeviceInfoList = _fn(_setupapi, "SetupDiDestroyDeviceInfoList", W.BOOL, W.HANDLE)
    CM_Get_Parent = _fn(_cfgmgr, "CM_Get_Parent", W.DWORD, _P(W.DWORD), W.DWORD, W.ULONG)
    CM_Get_Device_IDW = _fn(_cfgmgr, "CM_Get_Device_IDW", W.DWORD, W.DWORD, W.LPWSTR, W.ULONG, W.ULONG)
    CM_Get_DevNode_Registry_PropertyW = _fn(_cfgmgr, "CM_Get_DevNode_Registry_PropertyW", W.DWORD, W.DWORD,
                                            W.ULONG, _P(W.ULONG), _VP, _P(W.ULONG), W.ULONG)
    CreateFileW = _fn(_kernel, "CreateFileW", W.HANDLE, W.LPCWSTR, W.DWORD, W.DWORD, _VP, W.DWORD, W.DWORD,
                      W.HANDLE)
    CloseHandle = _fn(_kernel, "CloseHandle", W.BOOL, W.HANDLE)
    CreateEventW = _fn(_kernel, "CreateEventW", W.HANDLE, _VP, W.BOOL, W.BOOL, W.LPCWSTR)
    DeviceIoControl = _fn(_kernel, "DeviceIoControl", W.BOOL, W.HANDLE, W.DWORD, _VP, W.DWORD, _VP, W.DWORD,
                          _P(W.DWORD), _P(OVERLAPPED))
    WriteFile = _fn(_kernel, "WriteFile", W.BOOL, W.HANDLE, _VP, W.DWORD, _P(W.DWORD), _P(OVERLAPPED))
    ReadFile = _fn(_kernel, "ReadFile", W.BOOL, W.HANDLE, _VP, W.DWORD, _P(W.DWORD), _P(OVERLAPPED))
    WaitForSingleObject = _fn(_kernel, "WaitForSingleObject", W.DWORD, W.HANDLE, W.DWORD)
    CancelIoEx = _fn(_kernel, "CancelIoEx", W.BOOL, W.HANDLE, _P(OVERLAPPED))
    GetOverlappedResult = _fn(_kernel, "GetOverlappedResult", W.BOOL, W.HANDLE, _P(OVERLAPPED), _P(W.DWORD),
                              W.BOOL)
    INVALID_HANDLE = W.HANDLE(-1).value

def _need_ctypes():
    if CTYPES_ERROR is not None:
        raise SystemExit("this Python cannot load ctypes (%s), which the tools need on Windows. Smart App Control "
                         "can block the DLLs of unsigned Python builds (for example the ones uv installs): use "
                         "Python from python.org or the Microsoft Store." % CTYPES_ERROR)

def _open(path, access, flags=0):
    """CreateFile on a HID collection; None if it cannot be opened (e.g. removed since the enumeration)."""
    _need_ctypes()
    h = CreateFileW(path, access, FILE_SHARE_READ_WRITE, None, OPEN_EXISTING, flags, None)
    return None if h in (None, INVALID_HANDLE) else h

# --- device discovery -----------------------------------------------------------------------------------------
def _devnode_id(devinst):
    buf = ctypes.create_unicode_buffer(512)
    return buf.value if CM_Get_Device_IDW(devinst, buf, len(buf), 0) == 0 else ""

def _devnode_strings(devinst, prop):
    """A string or string-list property of a device node ([] if it has none)."""
    buf = ctypes.create_unicode_buffer(1024)
    n = W.ULONG(ctypes.sizeof(buf))
    if CM_Get_DevNode_Registry_PropertyW(devinst, prop, None, buf, ctypes.byref(n), 0) != 0:
        return []
    return [s for s in buf[:n.value // 2].split("\0") if s]

def _usb_device(devinst):
    """The USB device above a HID collection: HID\\... -> USB\\VID_...&MI_01\\... -> USB\\VID_...\\...
    Return (device node, instance ID)."""
    while True:
        inst = _devnode_id(devinst)
        if not (inst.upper().startswith("HID\\") or "&MI_" in inst.upper()):
            return devinst, inst
        parent = W.DWORD()
        if CM_Get_Parent(ctypes.byref(parent), devinst, 0) != 0:
            return devinst, inst
        devinst = parent.value

def _collections(ids):
    """Return [(path, iface1, UsbDevice)] for the present HID collections of the (vid, pid) pairs in ids."""
    _need_ctypes()
    guid = GUID()
    HidD_GetHidGuid(ctypes.byref(guid))
    hdev = SetupDiGetClassDevsW(ctypes.byref(guid), None, None, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE)
    if hdev in (None, INVALID_HANDLE):
        raise ctypes.WinError(ctypes.get_last_error())
    hits = []
    try:
        i = 0
        while True:
            ifd = SP_DEVICE_INTERFACE_DATA(cbSize=ctypes.sizeof(SP_DEVICE_INTERFACE_DATA))
            if not SetupDiEnumDeviceInterfaces(hdev, None, ctypes.byref(guid), i, ctypes.byref(ifd)):
                break
            i += 1
            need = W.DWORD()
            SetupDiGetDeviceInterfaceDetailW(hdev, ctypes.byref(ifd), None, 0, ctypes.byref(need), None)
            buf = ctypes.create_string_buffer(max(need.value, 8))
            ctypes.c_uint32.from_buffer(buf).value = 8 if ctypes.sizeof(ctypes.c_void_p) == 8 else 6   # cbSize
            dinfo = SP_DEVINFO_DATA(cbSize=ctypes.sizeof(SP_DEVINFO_DATA))
            if not SetupDiGetDeviceInterfaceDetailW(hdev, ctypes.byref(ifd), buf, len(buf), None,
                                                    ctypes.byref(dinfo)):
                continue
            path = ctypes.wstring_at(ctypes.addressof(buf) + 4)
            m = re.search(r"vid_([0-9a-f]{4})&pid_([0-9a-f]{4})", path, re.I)
            if not m or (int(m.group(1), 16), int(m.group(2), 16)) not in ids:
                continue
            h = _open(path, 0)                      # no access: attributes and caps only, nothing sent
            if h is None:
                continue
            try:
                attrs, caps, pp = HIDD_ATTRIBUTES(Size=ctypes.sizeof(HIDD_ATTRIBUTES)), HIDP_CAPS(), _VP()
                if not (HidD_GetAttributes(h, ctypes.byref(attrs)) and HidD_GetPreparsedData(h, ctypes.byref(pp))):
                    continue
                HidP_GetCaps(pp, ctypes.byref(caps))
                HidD_FreePreparsedData(pp)
            finally:
                CloseHandle(h)
            vidpid = (attrs.VendorID, attrs.ProductID)
            if vidpid not in ids:
                continue
            mi = re.search(r"&mi_([0-9a-f]{2})", path, re.I)
            iface1 = bool(mi) and int(mi.group(1), 16) == 1 and (
                caps.UsagePage == IFACE1_PAGE[vidpid] if vidpid in IFACE1_PAGE else "&col" not in path.lower())
            devinst, inst = _usb_device(dinfo.DevInst)
            location = (_devnode_strings(devinst, CM_DRP_LOCATION_PATHS)
                        or _devnode_strings(devinst, CM_DRP_LOCATION_INFORMATION) or [inst])[0]
            tail = inst.split("\\")[-1]             # the serial, or an ID that Windows made (contains "&")
            hits.append((path, iface1, UsbDevice(location, inst, attrs.VendorID, attrs.ProductID,
                                                 attrs.VersionNumber, None if "&" in tail else tail, path)))
    finally:
        SetupDiDestroyDeviceInfoList(hdev)
    return hits

def find_nodes(vid, pid):
    """Return [(node, iface1, USB device)] for the HID collections of vid:pid."""
    return sorted(_collections({(vid, pid)}))

def usb_dir(dev):
    return dev

def usb_port(dev):
    """Location path and serial of a USB device (two boards differ in these)."""
    return "USB %s%s" % (dev.location, ", serial %s" % dev.serial if dev.serial else "")

def usb_attrs(dev):
    out = {"idVendor": "%04x" % dev.vid, "idProduct": "%04x" % dev.pid, "bcdDevice": "%04x" % dev.bcd,
           "manufacturer": None, "product": None, "serial": dev.serial}
    h = _open(dev.hidpath, 0)
    if h is not None:
        try:
            for key, fn in (("manufacturer", HidD_GetManufacturerString), ("product", HidD_GetProductString)):
                buf = ctypes.create_unicode_buffer(128)
                if fn(h, buf, ctypes.sizeof(buf)):
                    out[key] = buf.value
        finally:
            CloseHandle(h)
    return out

def usb_devices(vid, pid):
    """Return the USB devices vid:pid (any firmware; attributes only, nothing sent)."""
    devs = {}
    for _, _, dev in _collections({(vid, pid)}):
        devs.setdefault(dev.instance, dev)
    return sorted(devs.values())

def wait_usb_at(dev, ids, timeout):
    """Wait until a USB device with one of the (vid, pid) ids is at the location of dev. A reset board enumerates
    again at the same port, so another board with the same ID does not count. Return the id."""
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        for _, _, d in _collections(set(ids)):
            if d.location == dev.location:
                return d.vid, d.pid
        time.sleep(0.25)
    return None

def wait_access(path, timeout):
    """Wait until a new collection can be opened. The first time a device with a new USB ID is connected, Windows
    installs the HID driver for it, which takes some seconds."""
    deadline = time.monotonic() + timeout
    while True:
        h = _open(path, GENERIC_READ_WRITE, FILE_FLAG_OVERLAPPED)
        if h is not None:
            CloseHandle(h)
            return True
        if time.monotonic() >= deadline:
            return False
        time.sleep(0.1)

# --- transport ------------------------------------------------------------------------------------------------
class Node:
    """An open HID collection: handle, report lengths (with the report ID byte), event for overlapped I/O."""
    def __init__(self, path):
        self.h = _open(path, GENERIC_READ_WRITE, FILE_FLAG_OVERLAPPED)
        if self.h is None:
            raise SystemExit("cannot open %s: %s" % (path, ctypes.FormatError(ctypes.get_last_error())))
        caps, pp = HIDP_CAPS(), _VP()
        if HidD_GetPreparsedData(self.h, ctypes.byref(pp)):
            HidP_GetCaps(pp, ctypes.byref(caps))
            HidD_FreePreparsedData(pp)
        self.inp, self.out, self.feat = (caps.InputReportByteLength, caps.OutputReportByteLength,
                                         caps.FeatureReportByteLength)
        self.event = CreateEventW(None, True, False, None)

def open_node(path):
    return Node(path)

def close_node(node):
    CloseHandle(node.event)
    CloseHandle(node.h)

def _io(node, start, timeout):
    """Run the overlapped call start(overlapped). Return the byte count, or None if it did not complete within
    timeout seconds (it is then canceled)."""
    ov = OVERLAPPED(hEvent=node.event)
    if not start(ctypes.byref(ov)):
        err = ctypes.get_last_error()
        if err != ERROR_IO_PENDING:
            raise ctypes.WinError(err)
    n = W.DWORD()
    if WaitForSingleObject(node.event, int(timeout * 1000)) != WAIT_OBJECT_0:
        CancelIoEx(node.h, ctypes.byref(ov))
        GetOverlappedResult(node.h, ctypes.byref(ov), ctypes.byref(n), True)    # until the cancel completes
        return None
    if not GetOverlappedResult(node.h, ctypes.byref(ov), ctypes.byref(n), False):
        raise ctypes.WinError(ctypes.get_last_error())
    return n.value

def _report(node, data, length, what):
    if len(data) != length:
        raise OSError(errno.EINVAL, "%s report of %d bytes; the device's has %d" % (what, len(data), length))
    return ctypes.create_string_buffer(bytes(data), length)

def set_feature(node, data):
    buf = _report(node, b"\x00" + data, node.feat, "feature")     # report ID 0: 64-byte SET_REPORT
    if _io(node, lambda ov: DeviceIoControl(node.h, IOCTL_HID_SET_FEATURE, buf, len(buf), None, 0, None, ov),
           CONTROL_TIMEOUT) is None:
        raise TimeoutError(errno.ETIMEDOUT, "SET_FEATURE timed out")

def get_feature(node):
    buf = ctypes.create_string_buffer(node.feat)                  # byte 0: report ID 0
    if _io(node, lambda ov: DeviceIoControl(node.h, IOCTL_HID_GET_FEATURE, buf, len(buf), buf, len(buf), None,
                                            ov), CONTROL_TIMEOUT) is None:
        raise TimeoutError(errno.ETIMEDOUT, "GET_FEATURE timed out")
    return buf.raw[1:]

def write_report(node, data):
    """Output report, report ID first. Without an OUT endpoint Windows sends it as a SET_REPORT, as Linux does."""
    buf = _report(node, data, node.out, "output")
    if _io(node, lambda ov: WriteFile(node.h, buf, len(buf), None, ov), CONTROL_TIMEOUT) is None:
        raise TimeoutError(errno.ETIMEDOUT, "output report timed out")

def read_report(node, timeout):
    """One input report (report ID first), or None if none arrives within timeout seconds."""
    buf = ctypes.create_string_buffer(node.inp)
    n = _io(node, lambda ov: ReadFile(node.h, buf, len(buf), None, ov), timeout)
    return None if n is None else buf.raw[:n]
