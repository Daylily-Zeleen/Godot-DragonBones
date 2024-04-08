# Godot-DragonBones Plugin

**This branch is created for submitting to Asset Library.**

![image](icon.png)

[点击查看中文说明](EADME.zh.md)。

A GDExtension plugin to add DragonBones for Godot.

## Links

* Godot: <https://godotengine.org>
* DragonBones: <http://dragonbones.com>

## Support Versions

* Godot 4.2 +
* DragonBones Pro 5.6

## Get plugin

1. Download from [release page](https://github.com/Daylily-Zeleen/Godot-DragonBones/releases).
2. Download from Assets Library.
3. Clone this repo and compile by youself.

## Run "demo" project

If you are not compile by youself, you should download this plugin and plug it into "demo".

## 说明

该仓库改进自[龙骨模块](https://github.com/sanja-sa/gddragonbones)。

### 改进内容

1. 改为4.x用的GDExtension。
2. 实现编辑器导入插件以供自动导入龙骨相关文件。
3. 导入资源为`DragonBonesFoctory`:
   * 单个工厂资源可以指定多个龙骨数据和图集描述数据文件
   * 可在`DragonBones`节点中指定要从`DragonBonesFoctory`实例化的龙骨数据名称和相应的皮肤名称
4. `DragonBones`节点:
   * 播放动画相关的方法只对主`DragonBonesArmature`进行操作，停止播放则递归对所有`DragonBonesArmature`操作
   * 其他属性则对所有的`DragonBonesArmature`操作
    由于龙骨对动画的操作粒度是针对某一个`Armature`，因此不建议直接对`DragonBones`进行动画相关的控制。
5. `DragonBonesArmature`节点:
   * **由`DragonBones`根据设定从`DragonBonesFoctory`进行实例化，不应该手动创建**。
   * 在编辑器中,作为`DragonBones`的"main_armature"属性以`DragonBonesArmatureProxy`类型进行设置,如果有子Armature，则会用有一个"sub_armatures"属性可供编辑（所有一切编辑设置将会保存在场景数据中，实例化时将被正确设置倒相应的`DragonBonesArmature`上）。
   * **千万不要手动释放！否则将导致崩溃！**
6. `DragonBonesArmatureProxy`**仅供编辑器使用，不要自行实例化，也不要访问相关对象（如`DragonBones`的"main_armature"与`DragonBonesArmatureProxy`的"sub_armatures"属性。）**
7. 可从`DragonBonesArmature`访问其中的`DragonBonesSlot`和`DragonBonesBone`进行一些高级操作（该部分没有测试，个人项目没有需求）。
8. 没有时间编写更详细的说明，具体请翻看"src/"下的源码。

## 其他

该仓库为个人项目使用，反正龙骨也已经跑路了，仅供给想在Godot中使用现存龙骨资源的人。
本人对龙骨中的概念也并不熟悉，是翻看源码一点改的，如果有些依赖关系搞混了也请多包涵。
改进的内容也已经大大超出我个人所需了，应该不会再附加什么改进了。
不过仍然欢迎提交修复和改进pr。
