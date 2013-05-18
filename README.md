修改时间： 2013/04/23
版本：	2-0 beta
修改人员： hpghy
修改bug： 
1、修改makefile文件,添加-Werror
2  log system
3、采用ETmode
4、修改accept_handle的返回值处理。不论accept_handle返回什么，都不能把监听conn从epoll中删除，否则导致websshproxy无法监听新的请求。
5、修改read_buffer/write_buffer, 把errno设置到全局变量中。
6、修改mod_connection代码的放置位置，需要在return之前设置epoll_mod_connection.
7、修改read_client_handle,read_buffer没有数据就不要处理，否则导致工作进程异常死掉
8、针对关闭链接后，还有很多close_wait状态，修改了read/write-client/server中的shutdown顺序
9、如果连接VM失败，发送一个错误页面
10、重新搭建整个源代码文件结构
11、单进程内存泄漏测试，多进程测试

记录时间：2013/05/18
通过部署，发现新的问题：
1、errorhtml有的时候无法显示；
2、输入错误的vm ip，有的时候可以连接前一个正常的vm，也就是当前server与url中的vm不匹配

修改内容：
1、修改conn_t字段，记录ip和端口号。
2、修改extract_ip,和server_conn不为空的逻辑,判断当前server是否与url中的vm-ip匹配

