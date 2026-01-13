all: server client

# 构建服务器
server:
	$(MAKE) -f Makefile.server
	rm -f spdk_server.d spdk_server.o

# 构建客户端
client:
	$(MAKE) -f Makefile.client

# 清理所有
clean:
	$(MAKE) -f Makefile.server clean
	$(MAKE) -f Makefile.client clean
