extends Zble


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	#start_scan()
	#notified.connect(notifyz)
	#services_update.connect(su)
	#device_disconnected.connect(ds)
	var w = Window.new()
	w.title = "z"
	w.position = Vector2i(50,50)
	w.always_on_top = true
	add_child(w)
	w.show()
	print(find_window_id_by_title("z"))
	pass # Replace with function body.
func ds():
	print("device_disconnected")
func su(s:String):
	print(s)
func notifyz(data:PackedByteArray):
	print(data)
	print(parse_heart_rate_value(data))
func _input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_accept"):
		print(0)
		stop_scan()
		print(connect_to_device("c4:d8:38:5c:8d:05"))
	if event.is_action_pressed("ui_down"):
		print(1)
		print(get_services())
		subcribe(0)
	if event.is_action_pressed("ui_up"):
		print(find_window_id_by_title("z"))
		set_window_click_through(find_window_id_by_title("z"))
func parse_heart_rate_value(bytes: PackedByteArray) -> int:
	const HEART_RATE_VALUE_FORMAT: int = 0x1
	
	#var bytes := buffer  # PoolByteArray 自动处理二进制数据
	
	if bytes[0] & HEART_RATE_VALUE_FORMAT:
		# 处理 16 位有符号整数（小端序）
		return (bytes[2] << 8) | bytes[1]
	else:
		# 处理 8 位无符号整数
		return bytes[1]
