Larbin网络爬虫
==============

[See the English edition](/README-en.md)

版本：2.6.4

Larbin是一个网络爬虫，但是目前已经停止开发，最终的版本是2.6.3。
我将继续开发这一优秀的软件，并在Github上开放源代码。

开发概况：
----------

* 目前，我在Larbin v2.6.3上做了一些修改， 修复了许多编译时的警告，发现并修复了一个严重的bug。

* 使用了最新版的adns v1.3替换了原始Larbin v2.6.3中的旧版adns v1.1。

* 增加了定时器功能，能设置larbin的运行时间。

* 增加HighLevelWebServer模式，在larbin停止爬取后，webserver依然正常运行，反馈状态信息。

* 增加了一个配置选项ignoreRobots，它可以使Larbin在抽取网页时不受robots.txt的影响，你懂的。

内容提要：
----------

* [编译Larbin](#编译larbin)
* [配置Larbin](#配置larbin)
* [运行Larbin](#运行larbin)
* [运行环境](#运行环境)
* [联系我](#联系我)

###编译Larbin

查看options.h文件来进行功能选择，这些功能直接决定了Larbin的功能和工作方式，请仔细配置这个文件。每次修改该文件后需要重新编译Larbin以使其生效。

执行如下命令进行:

```bash
> ./configure
> make
```
如果configure时出现错误，确保已经安装了makedepend，在Debian/Ubuntu类操作系统下，执行下列命令进行安装：
```bash
> sudo apt-get install xutils-dev
```
你仍然需要注意一些常用程序是否安装，如g++，m4等，如果没有，在Debian/Ubuntu类操作系统下，执行下列命令进行安装：
```bash
> sudo apt-get install g++ m4
```
Adns 1.3 在编译时会用到lynx，如果出现缺失错误，用上述同样的方法进行安装。

###配置Larbin

配置larbin.conf。确保正确修改为你的Email地址。

部分站点会阻止爬虫，需要修改UserAgent来伪装成浏览器。

仔细逐行查看larbin.conf中的每一段话，它们将详细引导你如何正确配置Larbin并让它成功跑起来。

如果你想使用中文提示的配置文件，请配置larbin-cn.conf。

在doc文件夹下，有Larbin的html格式的详细文档。

###运行Larbin

确保你已经完成配置，然后运行：

```bash
> ./larbin
```
如果你使用的是中文配置文件larbin-cn.conf，那么你需要在运行时设置一下配置文件的文件名：
```bash
> ./larbin -c larbin-cn.conf
```
如果你自己编写了配置文件，那么也需要在运行时设置配置文件的文件名：
```bash
> ./larbin -c your_conf_filename
```

查看它的工作情况，访问http://localhost:8081/

###运行环境

Larbin主要在Linux下进行开发。

我已经在Linux和FreeBSD下测试成功。

它可能在其他的平台下无法正确编译，但是我将在后续的版本中使其支持更多的平台。
请向我汇报Larbin在任何平台下的工作情况。

###联系我

URL: https://github.com/ictxiangxin/larbin

Email: ictxiangxin@gmail.com

QQ: 405340537
