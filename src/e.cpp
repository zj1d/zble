#include "e.h"
#include <godot_cpp/core/class_db.hpp>
#include <windows.h>
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace godot;
using namespace winrt;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Foundation;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Devices::Bluetooth;
void Zble::_bind_methods() {
	ClassDB::bind_method(D_METHOD("start_scan"), &Zble::start_scan);
	ClassDB::bind_method(D_METHOD("stop_scan"), &Zble::stop_scan);
	ClassDB::bind_method(D_METHOD("is_ble_supported"), &Zble::is_ble_supported);
	ClassDB::bind_method(D_METHOD("connect_to_device", "macAddress"), &Zble::connect_to_device);
	ClassDB::bind_method(D_METHOD("get_gatt_services"), &Zble::get_gatt_services);
	ClassDB::bind_method(D_METHOD("get_services"), &Zble::get_services);
	ClassDB::bind_method(D_METHOD("subcribe", "service_index"), &Zble::subcribe);
	ClassDB::bind_method(D_METHOD("unsubcribe"), &Zble::unsubcribe);
	ClassDB::bind_method(D_METHOD("find_window_id_by_title", "childTitle"), &Zble::find_window_id_by_title);
	ClassDB::bind_method(D_METHOD("set_window_click_through", "id"), &Zble::set_window_click_through);
	ClassDB::bind_method(D_METHOD("unset_window_click_through", "id"), &Zble::unset_window_click_through);
	// ClassDB::bind_method(D_METHOD("on_advertisement_received", "watcher", "args"), &Zble::on_advertisement_received);
    ADD_SIGNAL(MethodInfo("device_found", PropertyInfo(Variant::DICTIONARY, "device")));
    ADD_SIGNAL(MethodInfo("device_update", PropertyInfo(Variant::DICTIONARY, "device")));
    ADD_SIGNAL(MethodInfo("services_update", PropertyInfo(Variant::STRING, "status")));
	ADD_SIGNAL(MethodInfo("device_disconnected"));
	ADD_SIGNAL(MethodInfo("device_connected"));
    ADD_SIGNAL(MethodInfo("device_update_name", PropertyInfo(Variant::DICTIONARY, "device")));
    ADD_SIGNAL(MethodInfo("notified", PropertyInfo(Variant::PACKED_BYTE_ARRAY, "data")));
	// 绑定其他方法...
}


// 获取当前进程的窗口句柄
static HWND GetCurrentProcessWindowHandle(String childTitle) {
    DWORD currentProcessId = GetCurrentProcessId();
    HWND hwnd = NULL;
    do {
        hwnd = FindWindowExW(NULL, hwnd, NULL, NULL);
        DWORD processId;
        GetWindowThreadProcessId(hwnd, &processId);

        if (processId == currentProcessId){
            wchar_t windowTitle[256];
            int length = GetWindowTextW(hwnd, windowTitle, sizeof(windowTitle) / sizeof(wchar_t));
            if (length > 0 && String(windowTitle) == childTitle) {
                return hwnd;
            }
		} 

    } while (hwnd != NULL);
    return NULL;
}

// 将 uint64_t 转换为 MAC 地址格式的字符串
static std::string FormatBluetoothAddress(uint64_t address) {
	std::ostringstream stream;
	stream << std::hex << std::setfill('0')
		   << std::setw(2) << ((address >> 40) & 0xFF) << ":"
		   << std::setw(2) << ((address >> 32) & 0xFF) << ":"
		   << std::setw(2) << ((address >> 24) & 0xFF) << ":"
		   << std::setw(2) << ((address >> 16) & 0xFF) << ":"
		   << std::setw(2) << ((address >> 8) & 0xFF) << ":"
		   << std::setw(2) << (address & 0xFF);
	return stream.str();
}
// 将 MAC 地址格式的字符串  转换为 uint64_t
static uint64_t ParseBluetoothAddress(std::string const &address) {
	uint64_t result = 0;
	std::istringstream stream(address);
	std::string byte;
	for (int i = 0; i < 6; ++i) {
		std::getline(stream, byte, ':');
		result <<= 8;
		result |= std::stoul(byte, nullptr, 16);
	}
	return result;
}
static std::string HStringToString(winrt::hstring const &hstr) {
	return winrt::to_string(hstr);
}

static bool TryParseSigDefinedUuid(guid const &uuid, uint16_t &shortId) {
	// UUIDs defined by the Bluetooth SIG are of the form
	// 0000xxxx-0000-1000-8000-00805F9B34FB.
	constexpr guid BluetoothGuid = { 0x00000000, 0x0000, 0x1000, { 0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB } };

	shortId = static_cast<uint16_t>(uuid.Data1);
	guid possibleBluetoothGuid = uuid;
	possibleBluetoothGuid.Data1 &= 0xFFFF0000;
	return possibleBluetoothGuid == BluetoothGuid;
}
static hstring GetServiceName(GattDeviceService const &service) {
	uint16_t shortId;

	guid uuid = service.Uuid();
	if (TryParseSigDefinedUuid(uuid, shortId)) {
		// Reference: https://developer.bluetooth.org/gatt/services/Pages/ServicesHome.aspx
		const static std::map<uint32_t, const wchar_t *> knownServiceIds = {
			{ 0x0000, L"None" },
			{ 0x1811, L"AlertNotification" },
			{ 0x180F, L"Battery" },
			{ 0x1810, L"BloodPressure" },
			{ 0x1805, L"CurrentTimeService" },
			{ 0x1816, L"CyclingSpeedandCadence" },
			{ 0x180A, L"DeviceInformation" },
			{ 0x1800, L"GenericAccess" },
			{ 0x1801, L"GenericAttribute" },
			{ 0x1808, L"Glucose" },
			{ 0x1809, L"HealthThermometer" },
			{ 0x180D, L"HeartRate" },
			{ 0x1812, L"HumanInterfaceDevice" },
			{ 0x1802, L"ImmediateAlert" },
			{ 0x1803, L"LinkLoss" },
			{ 0x1807, L"NextDSTChange" },
			{ 0x180E, L"PhoneAlertStatus" },
			{ 0x1806, L"ReferenceTimeUpdateService" },
			{ 0x1814, L"RunningSpeedandCadence" },
			{ 0x1813, L"ScanParameters" },
			{ 0x1804, L"TxPower" },
			{ 0xFFE0, L"SimpleKeyService" },
		};
		auto it = knownServiceIds.find(shortId);
		if (it != knownServiceIds.end()) {
			return it->second;
		}
	}
	return L"Custom service: " + to_hstring(uuid);
}

static hstring GetCharacteristicName(GattCharacteristic const &characteristic) {
	uint16_t shortId;

	guid uuid = characteristic.Uuid();
	if (TryParseSigDefinedUuid(uuid, shortId)) {
		// Reference: https://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicsHome.aspx
		const static std::map<uint32_t, const wchar_t *> knownCharacteristicIds = {
			{ 0x0000, L"None" },
			{ 0x2A43, L"AlertCategoryID" },
			{ 0x2A42, L"AlertCategoryIDBitMask" },
			{ 0x2A06, L"AlertLevel" },
			{ 0x2A44, L"AlertNotificationControlPoint" },
			{ 0x2A3F, L"AlertStatus" },
			{ 0x2A01, L"Appearance" },
			{ 0x2A19, L"BatteryLevel" },
			{ 0x2A49, L"BloodPressureFeature" },
			{ 0x2A35, L"BloodPressureMeasurement" },
			{ 0x2A38, L"BodySensorLocation" },
			{ 0x2A22, L"BootKeyboardInputReport" },
			{ 0x2A32, L"BootKeyboardOutputReport" },
			{ 0x2A33, L"BootMouseInputReport" },
			{ 0x2A5C, L"CSCFeature" },
			{ 0x2A5B, L"CSCMeasurement" },
			{ 0x2A2B, L"CurrentTime" },
			{ 0x2A08, L"DateTime" },
			{ 0x2A0A, L"DayDateTime" },
			{ 0x2A09, L"DayofWeek" },
			{ 0x2A00, L"DeviceName" },
			{ 0x2A0D, L"DSTOffset" },
			{ 0x2A0C, L"ExactTime256" },
			{ 0x2A26, L"FirmwareRevisionString" },
			{ 0x2A51, L"GlucoseFeature" },
			{ 0x2A18, L"GlucoseMeasurement" },
			{ 0x2A34, L"GlucoseMeasurementContext" },
			{ 0x2A27, L"HardwareRevisionString" },
			{ 0x2A39, L"HeartRateControlPoint" },
			{ 0x2A37, L"HeartRateMeasurement" },
			{ 0x2A4C, L"HIDControlPoint" },
			{ 0x2A4A, L"HIDInformation" },
			{ 0x2A2A, L"IEEE11073_20601RegulatoryCertificationDataList" },
			{ 0x2A36, L"IntermediateCuffPressure" },
			{ 0x2A1E, L"IntermediateTemperature" },
			{ 0x2A0F, L"LocalTimeInformation" },
			{ 0x2A29, L"ManufacturerNameString" },
			{ 0x2A21, L"MeasurementInterval" },
			{ 0x2A24, L"ModelNumberString" },
			{ 0x2A46, L"NewAlert" },
			{ 0x2A04, L"PeripheralPreferredConnectionParameters" },
			{ 0x2A02, L"PeripheralPrivacyFlag" },
			{ 0x2A50, L"PnPID" },
			{ 0x2A4E, L"ProtocolMode" },
			{ 0x2A03, L"ReconnectionAddress" },
			{ 0x2A52, L"RecordAccessControlPoint" },
			{ 0x2A14, L"ReferenceTimeInformation" },
			{ 0x2A4D, L"Report" },
			{ 0x2A4B, L"ReportMap" },
			{ 0x2A40, L"RingerControlPoint" },
			{ 0x2A41, L"RingerSetting" },
			{ 0x2A54, L"RSCFeature" },
			{ 0x2A53, L"RSCMeasurement" },
			{ 0x2A55, L"SCControlPoint" },
			{ 0x2A4F, L"ScanIntervalWindow" },
			{ 0x2A31, L"ScanRefresh" },
			{ 0x2A5D, L"SensorLocation" },
			{ 0x2A25, L"SerialNumberString" },
			{ 0x2A05, L"ServiceChanged" },
			{ 0x2A28, L"SoftwareRevisionString" },
			{ 0x2A47, L"SupportedNewAlertCategory" },
			{ 0x2A48, L"SupportedUnreadAlertCategory" },
			{ 0x2A23, L"SystemID" },
			{ 0x2A1C, L"TemperatureMeasurement" },
			{ 0x2A1D, L"TemperatureType" },
			{ 0x2A12, L"TimeAccuracy" },
			{ 0x2A13, L"TimeSource" },
			{ 0x2A16, L"TimeUpdateControlPoint" },
			{ 0x2A17, L"TimeUpdateState" },
			{ 0x2A11, L"TimewithDST" },
			{ 0x2A0E, L"TimeZone" },
			{ 0x2A07, L"TxPowerLevel" },
			{ 0x2A45, L"UnreadAlertStatus" },
			{ 0x2A5A, L"AggregateInput" },
			{ 0x2A58, L"AnalogInput" },
			{ 0x2A59, L"AnalogOutput" },
			{ 0x2A66, L"CyclingPowerControlPoint" },
			{ 0x2A65, L"CyclingPowerFeature" },
			{ 0x2A63, L"CyclingPowerMeasurement" },
			{ 0x2A64, L"CyclingPowerVector" },
			{ 0x2A56, L"DigitalInput" },
			{ 0x2A57, L"DigitalOutput" },
			{ 0x2A0B, L"ExactTime100" },
			{ 0x2A6B, L"LNControlPoint" },
			{ 0x2A6A, L"LNFeature" },
			{ 0x2A67, L"LocationandSpeed" },
			{ 0x2A68, L"Navigation" },
			{ 0x2A3E, L"NetworkAvailability" },
			{ 0x2A69, L"PositionQuality" },
			{ 0x2A3C, L"ScientificTemperatureinCelsius" },
			{ 0x2A10, L"SecondaryTimeZone" },
			{ 0x2A3D, L"String" },
			{ 0x2A1F, L"TemperatureinCelsius" },
			{ 0x2A20, L"TemperatureinFahrenheit" },
			{ 0x2A15, L"TimeBroadcast" },
			{ 0x2A1B, L"BatteryLevelState" },
			{ 0x2A1A, L"BatteryPowerState" },
			{ 0x2A5F, L"PulseOximetryContinuousMeasurement" },
			{ 0x2A62, L"PulseOximetryControlPoint" },
			{ 0x2A61, L"PulseOximetryFeatures" },
			{ 0x2A60, L"PulseOximetryPulsatileEvent" },
			{ 0xFFE1, L"SimpleKeyState" },
		};

		auto it = knownCharacteristicIds.find(shortId);
		if (it != knownCharacteristicIds.end()) {
			return it->second;
		}
	}

	hstring userDescription = characteristic.UserDescription();
	if (!userDescription.empty()) {
		return userDescription;
	}

	return L"Custom Characteristic: " + to_hstring(uuid);
}

static bool IsBluetoothLESupported() {
	try {
		// 尝试创建 BluetoothLEAdvertisementWatcher 实例
		BluetoothLEAdvertisementWatcher watcher;
		return true; // 如果没有抛出异常，则支持 BLE
	} catch (const winrt::hresult_error &ex) {
		printf("BLE 不支持，错误: %s ", winrt::to_string(ex.message()).c_str());

		return false; // 如果抛出异常，则不支持 BLE
	}
}
static bool IsBluetoothRadioOn(BluetoothAdapter const &adapter) {
	auto radioAsync = adapter.GetRadioAsync();
	auto radio = radioAsync.get(); // Synchronously wait for the result
	return radio.State() == winrt::Windows::Devices::Radios::RadioState::On;
}
Zble::Zble() {
	// Initialize any variables here.
	//time_passed = 0.0;
	devices = TypedArray<Dictionary>();
	services = TypedArray<Dictionary>();
}

// 检查 BLE 是否受支持
bool Zble::is_ble_supported() {
	// 检查 BLE 是否受支持
	if (!IsBluetoothLESupported()) {
		// UtilityFunctions::printerr("GDSQLite Error: Can't open specified json-file, file does not exist or is locked");
		UtilityFunctions::printerr(L"BLE 不受支持。\n");
		return false;
	}
	// 获取默认的蓝牙适配器
	BluetoothAdapter adapter = BluetoothAdapter::GetDefaultAsync().get();
	if (adapter == nullptr) {
		UtilityFunctions::printerr(L"没有找到蓝牙适配器。\n");
		return false;
	}
	// 检查蓝牙适配器是否打开
	if (!IsBluetoothRadioOn(adapter)) {
		UtilityFunctions::printerr(L"蓝牙适配器未打开。\n");
		return false;
	}
	return true;
}
// 在 Zble 类中定义一个成员函数作为回调函数
void Zble::on_advertisement_received(BluetoothLEAdvertisementWatcher const &, BluetoothLEAdvertisementReceivedEventArgs const &args) {
    // printf("%u", args.Advertisement().LocalName().size());
    std::string macAddress = FormatBluetoothAddress(args.BluetoothAddress());
    auto localName = args.Advertisement().LocalName();
    std::wstring deviceName = (!localName.empty() && localName.size() > 0) ? localName.c_str() : L"未知设备";
    // printf("%u %d %ls \n", localName.size(), (!localName.empty() && localName.size() > 0), deviceName.c_str());
    // printf("设备地址: %s\t 设备名称: %ls\n", macAddress.c_str(), deviceName.c_str());
    // printf("信号强度 (RSSI): %d dBm\n", args.RawSignalStrengthInDBm());
    // 假设 devices 是一个 std::vector<Dictionary>，其中 Dictionary 是 Godot 的字典类型
    for (int i = 0; i < devices.size(); i++) {
        // 将 macAddress 转换为 String 类型后进行比较
		Dictionary deviced = devices[i];
        if (deviced.has("macAddress") && deviced["macAddress"] == godot::String::utf8(macAddress.c_str())) {
            deviced["rssi"] = args.RawSignalStrengthInDBm();
			if ((!localName.empty() && localName.size() > 0) && deviceName != L"未知设备" && deviced["name"] != String(deviceName.c_str()) && String(deviceName.c_str()).length() > ((String)deviced["name"]).length()){
				deviced["name"] = String(deviceName.c_str());
				// UtilityFunctions::print(L"更新设备:"+ String(deviceName.c_str())+ L"\t信号强度: "+ String::num(args.RawSignalStrengthInDBm())+ L"\t地址: "+ godot::String::utf8(macAddress.c_str()));
				call_deferred("emit_signal", "device_update_name", deviced );
			}
			call_deferred("emit_signal", "device_update", deviced );
            return;
        }
    }

    // 如果未找到设备，则添加新设备
    godot::Dictionary newDevice;
    newDevice["macAddress"] = godot::String::utf8(macAddress.c_str());
    newDevice["rssi"] = args.RawSignalStrengthInDBm();
    newDevice["name"] = String(deviceName.c_str());
    devices.push_back(newDevice);
	// UtilityFunctions::print(L"发现设备:"+ String(deviceName.c_str())+ L"\t信号强度: "+ String::num(args.RawSignalStrengthInDBm())+ L"\t地址: "+ godot::String::utf8(macAddress.c_str()));
	call_deferred("emit_signal", "device_found", newDevice );
}
void Zble::on_scan_stopped(BluetoothLEAdvertisementWatcher const &, BluetoothLEAdvertisementWatcherStoppedEventArgs const &args) {
    UtilityFunctions::printerr(L"扫描已停止，原因: " + static_cast<int>(args.Error()));
}
void Zble::start_scan() {
	// 启动扫描
	devices.clear();
	// 设置扫描模式（Active 模式会请求设备发送更多数据）
	watcher.ScanningMode(BluetoothLEScanningMode::Active);
	// // 注册接收广告的事件处理程序
	// watcher.Received(nullptr);

	// // 注册停止扫描的事件处理程序
	// watcher.Stopped(nullptr);
	// 注册接收广告的事件处理程序
	watcher.Received({ this, &Zble::on_advertisement_received });

	// 注册停止扫描的事件处理程序
	watcher.Stopped({ this, &Zble::on_scan_stopped });

	// 启动扫描
	UtilityFunctions::print(L"开始扫描 BLE 设备...");
	watcher.Start();
}
bool Zble::connect_to_device(const String &macAddress) {
    try {
        // 检查 MAC 地址是否为空
        if (macAddress.is_empty()) {
            UtilityFunctions::printerr(L"MAC 地址为空，无法连接设备。\n");
            return false;
        }

        // 将 godot::String 转换为 std::string
        std::string macAddressStd = macAddress.utf8().get_data();

        // 调用 ParseBluetoothAddress 并连接设备
        device = BluetoothLEDevice::FromBluetoothAddressAsync(ParseBluetoothAddress(macAddressStd)).get();

        if (device == nullptr) {
            UtilityFunctions::printerr(L"无法找到指定的蓝牙设备。\n");
            return false;
        }
		device.ConnectionStatusChanged([this](BluetoothLEDevice const& sender, IInspectable const&) {
            if (sender.ConnectionStatus() == BluetoothConnectionStatus::Disconnected) {
                // UtilityFunctions::print(L"设备已断开连接。");
				call_deferred("emit_signal", "device_disconnected" );
            }
			else {
                // UtilityFunctions::print(L"设备已连接。");
				call_deferred("emit_signal", "device_connected" );
            }

        });
		
		


        UtilityFunctions::print(L"成功连接到设备: "+ macAddress);
		get_gatt_services();
        return true;
    } catch (const winrt::hresult_error &ex) {
        godot::String errorMessage = godot::String::utf8(winrt::to_string(ex.message()).c_str());
        UtilityFunctions::printerr(godot::String(L"连接设备时发生错误: ") + errorMessage);
        return false;
    }
}
// 将 guid 转换为字符串的辅助函数
static std::string GuidToString(const guid& uuid) {
    return winrt::to_string(winrt::to_hstring(uuid));
}
TypedArray<Dictionary> Zble::get_services(){
	return services.duplicate();
}
// 在 Zble 类的实现文件中
void Zble::on_characteristic_value_changed(GattCharacteristic const& characteristic, GattValueChangedEventArgs const& args) {
    auto buffer = args.CharacteristicValue();
    if (!buffer) {
        // printf("特征值为空。\n");
        return;
    }

    auto reader = DataReader::FromBuffer(buffer);
    uint32_t bufferLength = buffer.Length();
    // printf("特征值大小: %u 字节\n", bufferLength);

    std::vector<uint8_t> data(bufferLength);
    reader.ReadBytes(winrt::array_view<uint8_t>(data.data(), data.data() + data.size()));

    // printf("特征值变化: ");
    // for (uint32_t i = 0; i < bufferLength; ++i) {
    //     printf("%02X ", data[i]);
    // }
    // printf("\n");
	    // 将 std::vector<uint8_t> 转换为 godot::PackedByteArray
    godot::PackedByteArray packed_data;
	for (uint32_t i = 0; i < bufferLength; ++i) {
		packed_data.append(data[i]);
	}

	call_deferred("emit_signal", "notified",packed_data );
}

// 重载 find_window_id_by_title 函数以接受父窗口标题和子窗口标题
int64_t Zble::find_window_id_by_title( String childTitle) {


    // 获取当前进程的窗口句柄
    HWND hwnd = GetCurrentProcessWindowHandle(childTitle);
    if (hwnd == NULL) {
        return -1; // 或者其他错误码
    }
	else{
		return reinterpret_cast<int64_t>(hwnd);
	}

    // return static_cast<int>(FindChildWindowByTitle(hwnd, wideChildTitle));
    // return static_cast<int>(GetChildWindowIdByTitle(hwnd, wideChildTitle));
}
// 实现 set_window_click_through 函数
void Zble::set_window_click_through(int64_t id) {
	HWND hwnd = reinterpret_cast<HWND>(id);
    if (hwnd == NULL) {
        UtilityFunctions::printerr(L"无效的窗口句柄。");
        return;
    }

    // 获取当前窗口的扩展样式
    LONG_PTR current_style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    // 添加 WS_EX_LAYERED 和 WS_EX_TRANSPARENT 样式
    LONG_PTR new_style = current_style | WS_EX_LAYERED | WS_EX_TRANSPARENT;

    // 设置新的扩展样式
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, new_style);

    // 更新窗口样式
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    UtilityFunctions::print(L"窗口已设置为点击穿透。");
}
// 实现 unset_window_click_through 函数
void Zble::unset_window_click_through(int64_t id) {
    HWND hwnd = reinterpret_cast<HWND>(id);
    if (hwnd == NULL) {
        UtilityFunctions::printerr(L"无效的窗口句柄。");
        return;
    }

    // 获取当前窗口的扩展样式
    LONG_PTR current_style = GetWindowLongPtr(hwnd, GWL_EXSTYLE);

    // 移除 WS_EX_LAYERED 和 WS_EX_TRANSPARENT 样式
    LONG_PTR new_style = current_style & ~(WS_EX_LAYERED | WS_EX_TRANSPARENT);

    // 设置新的扩展样式
    SetWindowLongPtr(hwnd, GWL_EXSTYLE, new_style);

    // 更新窗口样式
    SetWindowPos(hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

    UtilityFunctions::print(L"窗口已取消点击穿透。");
}
bool Zble::subcribe(int service_index) {
	if(characteristic == nullptr){
	characteristic = characteristics[service_index];
	characteristic.ValueChanged([this](GattCharacteristic const& characteristic, GattValueChangedEventArgs const& args) {
		on_characteristic_value_changed(characteristic, args);
	});
	}
	
    GattCommunicationStatus status = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
        GattClientCharacteristicConfigurationDescriptorValue::Notify).get();

    if (status != GattCommunicationStatus::Success) {
        UtilityFunctions::printerr(L"无法启动通知，状态: "+ static_cast<int>(status));
        return false;
    }
    else
    {
        UtilityFunctions::print(L"通知已启动！");

		return true;
    }

}
bool Zble::unsubcribe() {
	if(characteristic == nullptr){
		return true;
	}
	GattCommunicationStatus status = characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
        GattClientCharacteristicConfigurationDescriptorValue::None).get();

    if (status != GattCommunicationStatus::Success) {
       UtilityFunctions::printerr(L"无法停止通知，状态: "+ static_cast<int>(status));
        return false;
   }
   try {
		characteristic.ValueChanged(nullptr);
		characteristic = nullptr;
		return true;
	}
	catch (const winrt::hresult_error& ex) {
		UtilityFunctions::printerr(L"停止通知时发生错误: " + godot::String(winrt::to_string(ex.message()).c_str()));
		return false;
	}
   return true;
}
// Get GATT services
void Zble::get_gatt_services() {
	characteristics.clear();
	services.clear();
    // 启动获取 GATT 服务的异步操作
    auto operation = device.GetGattServicesAsync(BluetoothCacheMode::Uncached);

    // 注册完成处理程序
    operation.Completed([this](IAsyncOperation<GattDeviceServicesResult> const& operation, AsyncStatus status) {
        if (status == AsyncStatus::Completed) {
            auto result = operation.GetResults();

            if (static_cast<GattCommunicationStatus>(result.Status()) == GattCommunicationStatus::Success) {
                IVectorView<GattDeviceService> serviceList = result.Services();
                // UtilityFunctions::print(L"设备服务数量: " + godot::String::num(serviceList.Size()));

                for (auto const& service : serviceList) {
                    auto uuid = service.Uuid();
                    // 将 guid 转换为字符串并打印
                    // UtilityFunctions::print(L"服务 UUID: " + godot::String(GuidToString(uuid).c_str()));
                    // UtilityFunctions::print(godot::String(L"服务名称: ") + HStringToString(GetServiceName(service)).c_str());

                    // 获取服务的特征列表
                    auto characteristicsOperation = service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);
                    characteristicsOperation.Completed([this, service](IAsyncOperation<GattCharacteristicsResult> const& charOperation, AsyncStatus charStatus) {
                        if (charStatus == AsyncStatus::Completed) {
                            auto characteristicsResult = charOperation.GetResults();
                            if (static_cast<GattCommunicationStatus>(characteristicsResult.Status()) == GattCommunicationStatus::Success) {
                                auto characteristicsList = characteristicsResult.Characteristics();
                                // UtilityFunctions::print(L"服务特征数量: " + godot::String::num(characteristicsList.Size()));

                                for (GattCharacteristic const& characteristic : characteristicsList) {
                                    // UtilityFunctions::print(L"特征 UUID: " + godot::String(GuidToString(characteristic.Uuid()).c_str()));
                                    // UtilityFunctions::print(godot::String(L"特征名称: ") + HStringToString(GetCharacteristicName(characteristic)).c_str());
                                    // UtilityFunctions::print(L"特征属性: " + godot::String::num(static_cast<int>(characteristic.CharacteristicProperties())));
									if ((characteristic.CharacteristicProperties() & GattCharacteristicProperties::Notify) != GattCharacteristicProperties::None) {
										// UtilityFunctions::print(L"特征支持 Notify 属性");
										godot::Dictionary newservice;
										newservice["uuid"] = godot::String(GuidToString(characteristic.Uuid()).c_str());
										newservice["name"] = godot::String(HStringToString(GetCharacteristicName(characteristic)).c_str());
										// newservice["properties"] = godot::String::num(static_cast<int>(characteristic.CharacteristicProperties()));
										newservice["servicename"] = godot::String(HStringToString(GetServiceName(service)).c_str());
										newservice["index"] = characteristics.size();

										characteristics.push_back(characteristic);
										services.push_back(newservice);
										call_deferred("emit_signal", "services_update",newservice["name"]);
									}
                                    // if (HStringToString(GetCharacteristicName(characteristic)) == "HeartRateMeasurement") {
                                    //     printf("特征已找到！\n");
                                    // }
                                }
								
                            } else {
                                UtilityFunctions::printerr(L"无法获取服务特征，状态: " + godot::String::num(static_cast<int>(characteristicsResult.Status())));
								call_deferred("emit_signal", "services_update","error");
                            }
                        } else {
                            UtilityFunctions::printerr(L"获取服务特征时发生错误，状态: " + godot::String::num(static_cast<int>(charStatus)));
							call_deferred("emit_signal", "services_update","error");
                        }
                    });
                }
				
            } else {
                UtilityFunctions::printerr(L"无法获取设备服务，状态: " + godot::String::num(static_cast<int>(result.Status())));
				call_deferred("emit_signal", "services_update","error");
            }
        } else {
            UtilityFunctions::printerr(L"获取设备服务时发生错误，状态: " + godot::String::num(static_cast<int>(status)));
			call_deferred("emit_signal", "services_update","error");
        }
    });
}


// 停止扫描
void Zble::stop_scan() {
	watcher.Stop();
	UtilityFunctions::print(L"扫描已停止。");
}
Zble::~Zble() {
	// Add your cleanup here.
	unsubcribe();
	if (device != nullptr) {
		device.Close();
		device = nullptr;
	}
	watcher.Stop();
	// UtilityFunctions::print(L"Zble::~Zble()");

	watcher = nullptr;


}


