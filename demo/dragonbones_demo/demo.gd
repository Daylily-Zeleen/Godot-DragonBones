extends Node

const ZOOM_SENSITIVITY = 0.1
const DRAG_SENSITIVITY = 1.0

@onready var armature :DragonBonesArmatureView = $DragonBonesArmatureView
@onready var animation_process_mode_option_btn :OptionButton = %AnimationProcessModeOptionBtn
@onready var animation_option_btn :OptionButton = %AniamtionOptionBtn

@onready var camera :Camera2D = $Camera2D

@onready var _cam_zoom := camera.zoom

var _dragging := false

# Setup UI to control DragonBonesArmature's properties.
func _ready() -> void:
	_prepare_for_ci_test()

	# AnimationCallbackModeProcess
	for e: String in ClassDB.class_get_enum_constants(&"DragonBonesArmatureView", &"AnimationCallbackModeProcess"):
		animation_process_mode_option_btn.add_item(e.rsplit("_", false, 1)[1])
		animation_process_mode_option_btn.set_item_metadata(animation_process_mode_option_btn.item_count - 1, DragonBonesArmatureView[e])

	animation_process_mode_option_btn.item_selected.connect(_on_process_mode_option_btn_item_selected)
	for idx in range(animation_process_mode_option_btn.item_count):
		if animation_process_mode_option_btn.get_item_metadata(idx) == DragonBonesArmatureView.ANIMATION_CALLBACK_MODE_PROCESS_IDLE:
			animation_process_mode_option_btn.select(idx)
			_on_process_mode_option_btn_item_selected(idx)
			break

	# Animation
	animation_option_btn.add_item("[reset]")
	animation_option_btn.set_item_metadata(0, "")
	for anim in armature.get_animations():
		animation_option_btn.add_item(anim)
		animation_option_btn.set_item_metadata(animation_option_btn.item_count - 1, anim)

	animation_option_btn.item_selected.connect(_on_animation_option_btn_item_selected)
	animation_option_btn.select(0)
	_on_animation_option_btn_item_selected(0)

	# Flip
	%FlipXCheck.toggled.connect(func(toggled: bool): armature.flip_x = toggled)
	%FlipYCheck.toggled.connect(func(toggled: bool): armature.flip_y = toggled)
	%FlipXCheck.button_pressed = false
	%FlipYCheck.button_pressed = false

	# Debug
	%DebugCheck.toggled.connect(func(toggled: bool): armature.debug_draw_enabled = toggled)
	%DebugCheck.button_pressed = false

	# Active
	%ActiveCheck.toggled.connect(func(toggled: bool): armature.active = toggled)
	%ActiveCheck.button_pressed = false

	# Time Scale
	%TimeScaleSpinBox.value_changed.connect(func(value: float): armature.animation_time_scale = value)
	%TimeScaleSpinBox.value = 1.0
	
	# Manual Advance
	%AdvanceBtn.pressed.connect(_on_advance_btn_pressed)

func _on_process_mode_option_btn_item_selected(index: int) -> void:
	armature.animation_callback_mode_process = animation_process_mode_option_btn.get_item_metadata(index)
	%AdvanceUI.visible = armature.animation_callback_mode_process == DragonBonesArmatureView.ANIMATION_CALLBACK_MODE_PROCESS_MANUAL


func _on_animation_option_btn_item_selected(index: int) -> void:
	var anim :String = animation_option_btn.get_item_metadata(index) as String
	if anim == armature.current_animation:
		return

	armature.current_animation = anim


func _on_advance_btn_pressed() -> void:
	assert(armature.animation_callback_mode_process == DragonBonesArmatureView.ANIMATION_CALLBACK_MODE_PROCESS_MANUAL)
	var delta :float = %AdvanceTime.value
	armature.advance(delta)

func _process(delta: float) -> void:
	camera.zoom = camera.zoom.slerp(_cam_zoom, 5.0 * delta)

func _unhandled_input(event: InputEvent) -> void:
	var mb := event as InputEventMouseButton
	if mb and not mb.is_echo() and mb.is_pressed():
		match mb.button_index:
			MOUSE_BUTTON_WHEEL_UP:
				_cam_zoom += Vector2(ZOOM_SENSITIVITY, ZOOM_SENSITIVITY)
			MOUSE_BUTTON_WHEEL_DOWN:
				_cam_zoom -= Vector2(ZOOM_SENSITIVITY, ZOOM_SENSITIVITY)
				if _cam_zoom.x < 0.0 or _cam_zoom.y < 0.0:
					_cam_zoom = Vector2.ZERO
			MOUSE_BUTTON_MIDDLE:
				_dragging = true

	if _dragging:
		var mm := event as InputEventMouseMotion
		if mm:
			camera.position -= mm.relative * DRAG_SENSITIVITY

func _input(event: InputEvent) -> void:
	var mb := event as InputEventMouseButton
	if mb and mb.button_index == MOUSE_BUTTON_MIDDLE and  not mb.is_echo() and not mb.is_pressed():
			_dragging = false


func _prepare_for_ci_test() -> void:
	if not OS.get_cmdline_user_args().has("--auto-exit"): return

	# Enable all debug draw.
	armature.debug_draw_enabled = true
	armature.debug_draw_visible_bone = true
	armature.debug_draw_visible_mesh = true
	armature.debug_draw_visible_bone_name = true

	# Auto quit after 3 sec.
	get_tree().create_timer(3.0).timeout.connect(get_tree().quit)
