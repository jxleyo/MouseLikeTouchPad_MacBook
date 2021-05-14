# MouseLikeTouchPad_Mac0.1
仿鼠标式触摸板macbook on windows10驱动

仿鼠标式触摸板是一种模拟鼠标功能的触摸板技术实现，当前的逻辑实现版本是发明人基于仿鼠标触摸板专利技术根据人手指操作时自然状态再次优化改进而成，3指完成鼠标左键/右键/中键和指针的操作，手指与鼠标的各个按键等部件的功能一一对应，其中的中指对应鼠标的指针定位器，食指对应鼠标左键和中键（食指与中指分开时定义为鼠标左键，食指与中指并拢时定义为鼠标中键），无名指对应鼠标右键，中无名并拢时对应鼠标垂直滚轮和水平滚轮。

手指与鼠标按键功能对应关系： 中指对应鼠标的指针定位器； 食指对应鼠标左键和中键，根据鼠标左键、中键普遍用食指操作的习惯及实际可行性来确定的，食指与中指分开时食指操作定义为鼠标左键，食指与中指并拢时食指操作定义为鼠标中键，食指与中指由分开状态切换为并拢状态时系统判定鼠标左键按下状态不变同时中键判定为按下状态，食指与中指由并拢状态切换为分开状态时系统判定鼠标中键释放同时左键判定为按下状态，食指与中指由并拢状态或者分开状态切换到食指离开触摸板时系统判定鼠标左键中键都是释放，； 无名指对应鼠标右键；根据鼠标右键、屏幕滚动操作的习惯及实际可行性来确定的，无名指与中指分开时无名指操作定义为鼠标右键，无名指与中指并拢时无名指中指合并操作定义为鼠标垂直或水平滚轮同时中指的指针定义保持不变但不会移动指针直到两指分开或者离开触摸板（正常中指无名指并拢滑动操作舒适性操作性最高并且一般中指比无名指更早接触到触摸板使得系统判定不会有歧义）。

多点电容式触摸板根据触摸点形状间距很容易就能冲硬件底层排除掉手掌的误触（打字时触摸板支撑手掌的接触面为大面积连续近似月牙形状），正常触摸板操作时手指面积小且形状圆润更规则，并且单个点误触只是指针移动不会影响打字输入和按键操作；

本人尝试过联系笔记本厂商、触摸板方案商甚至微软来提供逻辑实现方法但是没有渠道联系不上或者人家及大量IT人士认为没有价值所以最终决定自己开发，由于本人以前完全没有windows驱动开发经历所以只能查找现成的触摸板驱动代码改造完成，原本想开发触摸板硬件替换笔记本的触摸板但发现没有硬件资料和资金及技术实现难度太大转而在现有笔记本硬件上开发触摸板驱动，主流windows笔记本的触摸板驱动（因为windows系统比macos好用、笔记本硬件价格便宜并且普及适应性广），但是全球各大中外网络只找到了macbook笔记本硬件的windows版本驱动代码，期望能够修改后移植到普通windows笔记本硬件下。 本驱动的基本框架为完全参考fanxiushu/kmouse_filter-AppleSPITrack-driver的实现（https://github.com/fanxiushu/kmouse_filter-AppleSPITrack-driver，感谢此作者的开源代码），仿鼠标式触摸板的操作逻辑实现代码MouseLikeTouchPad_parse函数则是全部自己完成，本驱动替换Apple的AppleSPITrackpad驱动在macbook pro 2017版13寸不带bar的机器测试成功（其他版本估计也是可以的因为代码内没有和触摸板硬件相关的代码），安装后基本能够达到预期的逻辑实现目标。 目前已经写了多个版本的逻辑算法实现在macbook硬件windows系统都存在各种奇怪的问题，指针跟踪有鼠标回报率莫名被限制在64/65hz以内，食指/无名指和中指并拢操作时指针坐标突发性漂移（macbook硬件下原生macos系统和windows系统都同样存在但因为操作逻辑和仿鼠标触摸板不同不会有不良影响，初步怀疑是触摸板硬件的firmware固件设计上存在瑕疵无法准确通过双指并拢时电容出生的两个手指坐标），mac0.1版驱动逻辑上我就知道了由于底层读取触摸板数据的速率较低无法做到手指坐标的快速跟踪所以极快速滑动指针时会失去响应（指针的手指坐标丢失），0.2版本修改了坐标跟踪算法可以快速移动手指不掉帧但是左键/右键和指针一起拖动时有大概率会极度卡顿（但实际CPU占有率很低，系统中断占用也很低，找不出原因所在），0.3版本使用了触摸板报告中struct SPI_TRACKPAD_FINGER结构体的触摸板点初始坐标OriginalX、OriginalY作为手指跟踪方案;  //触摸时的初始坐标，按下后，这个数字不变，可以用于追踪手指另外最重要的本意是安装到普通windows笔记本硬件上完全读取不到触摸板报告数据，macbook触摸板是SPI总线连接，我特意买了2台较大尺寸触摸板的windows笔记本新产品（matebook14 2020intel版和联想yoga14s 2021锐龙5800H版）作为开发用途，win笔记下触摸板是i2c总线连接，尽管剔除了SetSpecialFeature函数这个总线相关的代码macbook下windows驱动同样可以良好运行（经过测试SetSpecialFeature函数在windows休眠后唤醒的时候会使用到不然触摸板会失效），windows笔记本硬件下设备管理器-人体学输入设备对应的触摸板i2c设备手动安装成功但是经过各种测试都读取不到触摸板数据，CompletionRoutine例程下IOCTL_HID_READ_REPORT向下发送IoCallDriver读取报告操作始终返回0xc0000010即STATUS_INVALID_DEVICE_REQUEST错误，尝试过ioctl同步方法和异步方法都不行，并且都是和总线和硬件指令操作无关的只在kmdf环境的hid ioctl控制指令来完成，设备管理器-人体学输入设备对应的触摸板i2c设备属性中驱动程序标签页的禁用设备按钮变灰、事件标签页显示成功安装MouseLikeTouchPad.inf但始终未启动设备mshidkmdf（macbook硬件windows下为已启动mshidkmdf）,并在查看所有事件中发现来源Kernel-Pnp为411事件ID的电源管理错误有查看系统设备树下i2c父系设备Serial io Host Controller串行控制器属性中详细详细的电源数据的当前电源状态始终为D3不变（不管手接触触摸板都是电源关闭状态，macbook下是会随着手接触触摸板变为D0），然后Serial io Host Controller串行控制器属性中电源管理的允许计算机关闭此设备以节约电源选项取消勾选可以改变为D0状态不变但依旧0xc0000010错误读取不到触摸板数据，测试CompletionRoutine例程下向下发送IOCTL_HID_GET_DEVICE_ATTRIBUTES等其他指令操作依旧0xc0000010，怀疑是mshidkmdf服务未启动的原因但是找不到办法使之运行，因为本人实在是没有驱动开发基础只懂的一点编程皮毛完全摸不到头绪如果有知道解决方法的windows驱动大神请马上和本人联系有报酬重谢，同时因为macbook硬件下的驱动也不完善就没有签名认证了，有兴趣的朋友可以关闭强制驱动签名来安装试用，本驱动会长期改进优化，未来windows笔记本下成功开发出来后本人会自费购买签名证书后免费发布驱动供广大用户使用，盈利不是本人的目的，能够普及仿鼠标式触摸板技术是我最大的心愿。

驱动安装方法： windows开始菜单右键点搜索cmd、找到命令提示符右键点管理员运行、输入bcdedit/set testsigning on关闭驱动强制签名后重启， Release\MouseLikeTouchPad目录中确保MouseLikeTouchPad.inf、MouseLikeTouchPad.sys、MouseLikeTouchPad.cat在同一文件夹下， 右键MouseLikeTouchPad.inf文件选择安装后点击始终安装此驱动程序软件， windows开始菜单右键打开设备管理器、点开人体学输入设备找到Apple SPI Trackpad打开、属性页面的驱动程序标签下点更新驱动程序后选浏览我的计算机以查找驱动程序软件、点让我从计算机上的可用驱动程序列表中选取、下方显示有MouseLikeTouchPad选中后下一步即可完成安装。

卸载方法： 设备管理器》人体学输入设备》MouseLikeTouchPad右键卸载设备》选中删除此设备的驱动程序软件复选框后卸载即可。

仿鼠标式触摸板专利作者：大余县乐友电子科技部 黄瑞平 相关资料： 笔记本电脑革命-仿鼠标式触摸板 视频演示 https://www.bilibili.com/video/av48712199

笔记本模式笔记本电脑革命-仿鼠标式触摸板 笔记本模式4K 视频演示 https://www.bilibili.com/video/av64713037

笔记本电脑办公神器-随时随地轻松操控 https://www.bilibili.com/read/cv2680213

仿鼠标式触摸板-笔记本电脑技术革命 https://www.bilibili.com/read/cv2522582

仿鼠标式触摸板技术原理说明书中文版 https://www.bilibili.com/read/cv2522031
