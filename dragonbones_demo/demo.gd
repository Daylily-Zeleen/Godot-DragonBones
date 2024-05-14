extends Node


@onready var dragonbones :DragonBones = $DragonBones
@onready var animation_process_mode_option_btn :OptionButton = %AnimationProcessModeOptionBtn
@onready var animation_option_btn :OptionButton = %AniamtionOptionBtn


@onready var _armature :DragonBonesArmature = dragonbones.armature


# Setup UI to control DragonBonesArmature's properties.
func _ready() -> void:
	# AnimationCallbackModeProcess
	for e: String in ClassDB.class_get_enum_constants(&"DragonBonesArmature", &"AnimationCallbackModeProcess"):
		animation_process_mode_option_btn.add_item(e.rsplit("_", false, 1)[1])
		animation_process_mode_option_btn.set_item_metadata(animation_process_mode_option_btn.item_count - 1, DragonBonesArmature[e])

	animation_process_mode_option_btn.item_selected.connect(_on_process_mode_option_btn_item_selected)
	for idx in range(animation_process_mode_option_btn.item_count):
		if animation_process_mode_option_btn.get_item_metadata(idx) == DragonBonesArmature.ANIMATION_CALLBACK_MODE_PROCESS_IDLE:
			animation_process_mode_option_btn.select(idx)
			_on_process_mode_option_btn_item_selected(idx)
			break

	# Animation
	animation_option_btn.add_item("[reset]")
	animation_option_btn.set_item_metadata(0, "")
	for anim in _armature.get_animations():
		animation_option_btn.add_item(anim)
		animation_option_btn.set_item_metadata(animation_option_btn.item_count - 1, anim)

	animation_option_btn.item_selected.connect(_on_animation_option_btn_item_selected)
	animation_option_btn.select(0)
	_on_animation_option_btn_item_selected(0)

	# Flip
	%FlipXCheck.toggled.connect(func(toggled: bool): _armature.flip_x = toggled)
	%FlipYCheck.toggled.connect(func(toggled: bool): _armature.flip_y = toggled)
	%FlipXCheck.button_pressed = false
	%FlipYCheck.button_pressed = false

	# Debug
	%DebugCheck.toggled.connect(func(toggled: bool): _armature.debug = toggled)
	%DebugCheck.button_pressed = false

	# Active
	%ActiveCheck.toggled.connect(func(toggled: bool): _armature.active = toggled)
	%ActiveCheck.button_pressed = true

	# Time Scale
	%TimeScaleSpinBox.value_changed.connect(func(value: float): _armature.time_scale = value)
	%TimeScaleSpinBox.value = 1.0
	
	# Manual Advance
	%AdvanceBtn.pressed.connect(_on_advance_btn_pressed)

func _on_process_mode_option_btn_item_selected(index: int) -> void:
	_armature.callback_mode_process = animation_process_mode_option_btn.get_item_metadata(index)
	%AdvanceUI.visible = _armature.callback_mode_process == DragonBonesArmature.ANIMATION_CALLBACK_MODE_PROCESS_MANUAL


func _on_animation_option_btn_item_selected(index: int) -> void:
	var anim :String = animation_option_btn.get_item_metadata(index) as String
	if anim == _armature.current_animation:
		return

	_armature.stop(_armature.current_animation, true)
	if not anim.is_empty():
		_armature.play(anim, -1)


func _on_advance_btn_pressed() -> void:
	assert(_armature.callback_mode_process == DragonBonesArmature.ANIMATION_CALLBACK_MODE_PROCESS_MANUAL)
	var delta :float = %AdvanceTime.value
	_armature.advance(delta)
	
