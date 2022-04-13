## SGS Godot client
### Note: data_received signal is expected to be connected with a function called data_received

extends Node
class_name SGSNetwork

var client : WebSocketClient = WebSocketClient.new()

signal data_received
signal server_connected
signal server_disconnected
signal lobby_connected
signal lobby_disconnected

var current_url : String = ""


func _ready():
	client.connect("connection_closed", self, "_connection_closed")
	client.connect("connection_error", self, "_connection_error")
	client.connect("connection_established", self, "_connection_established")
	client.connect("data_received", self, "_data_received")

func _connection_established(proto = ""):
	print("SGS: Connected to server: " + current_url)
	connected_to_sgs = true
	emit_signal("server_connected")

func _data_received():
	var raw = client.get_peer(1).get_packet().get_string_from_utf8()
	
	var res = JSON.parse(raw)
	if res.error != OK:
		prints("SGS: Unable to parse data:", raw)
		return
	
	var obj = res.result
	
	if obj.get("type", "error") == "success":
		if connecting_to_lobby:
			connecting_to_lobby = false
			connected_to_lobby = true
			current_lobby = obj.get("lobby")  # Should always be in packet
			current_id = obj.get("data", {}).get("player_id", -1)
			print("Debug: Set lobby to ", current_lobby)
			emit_signal("lobby_connected")
			
		
		if "is_leader" in obj.get("data"):
			lobby_leader = obj.get("data", {}).get("is_leader", false)
		return
	
	if obj.get("type", "error") == "error":
		prints("SGS: Error message:", obj)
	
	if obj.get("type", "error") == "initialization_data":
		_smart_emit_data_received(obj.get("data", {}))
	
	if obj.get("type", "error") == "data":
		_smart_emit_data_received(obj.get("data", {}))

var _queued_data : Array = []
func _smart_emit_data_received(data : Dictionary):
	# Check if current scene has connected signal to our data_received, if not,
	# queue it for next time
	var current_scene : Node = get_tree().get_current_scene()
	if self.is_connected("data_received", current_scene, "data_received"):
		for prev_data in _queued_data:
			emit_signal("data_received", prev_data)
		_queued_data.clear()
		emit_signal("data_received", data)
	else:
		_queued_data.append(data)

func _connection_closed(was_clean = false):
	prints("Closed:", was_clean)
	set_process(false)
	_reset_state_variables()
	emit_signal("server_disconnected")
	emit_signal("lobby_disconnected")

func _connection_error(was_clean = false):
	prints("Error:", was_clean)
	set_process(false)
	_reset_state_variables()
	emit_signal("server_disconnected")
	emit_signal("lobby_disconnected")

func _process(_delta):
	client.poll()
	
func _reset_state_variables():
	connected_to_sgs = false
	connecting_to_lobby = false
	connected_to_lobby = false
	lobby_leader = false
	current_lobby = ""
	current_game = ""
	current_id = -1

# State variables
var connected_to_sgs = false
var connecting_to_lobby = false
var connected_to_lobby = false
var lobby_leader = false
var current_lobby = ""
var current_game = ""
var current_id = -1

func disconnect_from_server():
	if connected_to_server():
		client.disconnect_from_host()

func connect_to_server(address):
	var success = client.connect_to_url(address)
	current_url = address
	connected_to_sgs = false
	if success != OK:
		print("SGS: Unable to connect")
		_reset_state_variables()
		set_process(false)
	else:
		set_process(true)

func connected_to_server():
	return connected_to_sgs

func in_lobby():
	return connected_to_lobby

func connect_to_lobby(lobby_name, game_name):
	if not connected_to_server():
		return
	if connecting_to_lobby:
		return
	
	connecting_to_lobby = true
	current_game = game_name
	var message = {
		"type": "data",
		"data": {},
		"lobby": lobby_name,
		"game": game_name
	}
	var jstr = JSON.print(message)
	client.get_peer(1).put_packet(jstr.to_utf8())

func send_data(obj):
	if not connected_to_server() or not in_lobby():
		return false
	var message = {
		"type": "data",
		"data": obj,
		"lobby": current_lobby,
		"game": current_game
	}
	var jstr = JSON.print(message)
	client.get_peer(1).put_packet(jstr.to_utf8())
	return true

func send_initialization(obj):
	if not connected_to_server() or not in_lobby() or not is_leader():
		return false
	var message = {
		"type": "initialization_data",
		"data": obj,
		"lobby": current_lobby,
		"game": current_game
	}
	var jstr = JSON.print(message)
	client.get_peer(1).put_packet(jstr.to_utf8())
	return true

func is_leader():
	return (lobby_leader and connected_to_lobby)

func player_id():
	return current_id
