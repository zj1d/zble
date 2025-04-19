#ifndef GDEXAMPLE_H
#define GDEXAMPLE_H
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <godot_cpp/classes/node.hpp>
#include <winrt/Windows.Foundation.h>
#include <string>
#include <winrt/base.h>
#include <windows.h>
#include <unknwn.h> // 包含 IUnknown 的定义
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.Collections.h> 
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Devices.Radios.h>  
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace winrt;
using namespace Windows::Devices::Bluetooth::Advertisement;
using namespace Windows::Foundation;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Devices::Bluetooth;
namespace godot {

class Zble : public Node {
	GDCLASS(Zble, Node)

private:
	BluetoothLEAdvertisementWatcher watcher;
	BluetoothLEDevice device = nullptr;
	GattCharacteristic characteristic = nullptr;
	TypedArray<Dictionary> devices;
	TypedArray<Dictionary> services;
	std::vector<GattCharacteristic> characteristics;

	//std::vector<SimpleBLE::Peripheral> devices;

protected:
	static void _bind_methods();

public:
	Zble();
	~Zble();

	void on_advertisement_received(BluetoothLEAdvertisementWatcher const &, BluetoothLEAdvertisementReceivedEventArgs const &args);

	void on_scan_stopped(BluetoothLEAdvertisementWatcher const &, BluetoothLEAdvertisementWatcherStoppedEventArgs const &args);

	void start_scan();
	bool connect_to_device(const String &macAddress);
	void get_gatt_services();
	void stop_scan();
	bool is_ble_supported();
	TypedArray<Dictionary> get_services();
	void on_characteristic_value_changed(GattCharacteristic const &characteristic, GattValueChangedEventArgs const &args);
	int64_t find_window_id_by_title(String childTitle);
	void set_window_click_through(int64_t id);
	void unset_window_click_through(int64_t id);
	bool subcribe(int service_index);
	bool unsubcribe();
};

} //namespace godot

#endif
