# MouseLikeTouchPad_MacBook
仿鼠标式触摸板驱动程序macbook on windows10 touchpad driver

# MouseLikeTouchPad_MacBook
仿鼠标式触摸板驱动程序macbook on windows10 touchpad driver

仿鼠标式触摸板是一种模拟鼠标功能的触摸板技术实现，当前的逻辑实现版本是发明人基于仿鼠标触摸板专利技术根据人手指操作时自然状态再次优化改进而成，3指完成鼠标左键/右键/中键和指针的操作，手指与鼠标的各个按键等部件的功能一一对应，其中的中指对应鼠标的指针定位器，食指对应鼠标左键和中键（食指与中指分开时定义为鼠标左键，食指与中指并拢时定义为鼠标中键），无名指对应鼠标右键，中指无名指或者中指与食指2指一起快速触摸后滑动操作时对应鼠标垂直滚轮和水平滚轮，完全兼容windows原版三指和四指手势及系统自定义手势。

多点电容式触摸板根据触摸点接触面形状很容易解决手掌的误触（打字时触摸板支撑手掌的椭圆接触面的长宽比特征比正常手指大很容易排除过滤掉）,中键因为苹果的触控板硬件原因导致双指紧挨着首次接触触摸板时会有较大几率坐标漂移体验不太好（普通windows笔记本就基本不会漂移所以体验很好）；

本人于2012年左右就已经有这个想法但因为技术原因一直没有条件实现，最近机会成熟了才开始自己开发笔记本电脑的触摸板驱动，本驱动的基本框架为大部分参考fanxiushu/kmouse_filter-AppleSPITrack-driver的实现https://github.com/fanxiushu/kmouse_filter-AppleSPITrack-driver，少量参考https://github.com/imbushuo/mac-precision-touchpad的代码（感谢以上作者的开源代码），本驱动程序仅凭我一人之力历经近1年时间艰苦攻坚奋战开发而成并增加大量仿鼠标触摸板逻辑实现代码，仿鼠标式触摸板的操作逻辑实现代码MouseLikeTouchPad_parse函数则是全部自己完成，获取到手指触摸点数据并完美还原全部手势操作，本驱动替换Apple的AppleSPITrackpad驱动在macbook pro 2017版13寸不带bar的机器测试成功（其他版本估计也是可以的因为代码内没有和触摸板硬件相关的代码），安装后基本能够达到预期的逻辑实现目标， 驱动带有数字签名证书安装简便安全，欢迎大家免费下载使用。

另外普通windows笔记本版本也已经开发完成并发布，大家可以免费下载使用，目前主流的windows笔记本触控板硬件基本上采用了I2C总线连接并且系统自带默认驱动使得厂家不需要另外开发专用驱动程序，macbook触摸板是SPI总线连接，所以需要本专用版本驱动。如果您特别希望有安装更简单安全的正式电子签名证书版本的用户可以进入本人的bilibili个人主页充电赞助（几元到十几元任意）达到10人以上我会先购买第三方安全认证的代签名电子证书后发布，本驱动会长期改进优化，能够普及仿鼠标式触摸板技术是我最大的心愿。

驱动安装方法：  打开Installation Package目录，双击SetSensitivity.reg导入注册表或者确保系统设置里设备\触摸板页面的触摸板敏感度选项为最高敏感度（此设置保证键盘触摸板互不干扰且触摸板防止误触功能能正常运行），双击EVRootCA.reg导入注册表后弹出的数字签名证书安装窗口选择信任，右键MouseLikeTouchPad.inf文件选择安装后点击始终安装此驱动程序软件， windows开始菜单右键打开设备管理器、点开人体学输入设备找到Apple SPI Trackpad打开、属性页面的驱动程序标签下点更新驱动程序后选浏览我的计算机以查找驱动程序软件、点让我从计算机上的可用驱动程序列表中选取、下方显示有MouseLikeTouchPad选中后下一步即可完成安装。

卸载方法： 设备管理器》人体学输入设备》MouseLikeTouchPad右键卸载设备》选中删除此设备的驱动程序软件复选框后卸载即可。

仿鼠标式触摸板实机演示https://www.bilibili.com/video/BV1Bh411Y7aY

使用操作视频教程网址：
https://space.bilibili.com/409976933
https://www.youtube.com/channel/UC3hQyN-2ZL_q7pCKoASAblQ

