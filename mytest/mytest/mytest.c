#include <stdio.h>
#include "mytest.pb.h"
#include <pb.h>
#include <pb_encode.h>
#include <pb_decode.h>

// 解码的例子见tests\callbacks例子
// 注意编码encode的最后一个参数是 void * const *arg
bool string_encode_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	char* str = (char*)*arg;

	//char *str = "Hello world!";
	// 回调函数必须先调一下这个
	if (!pb_encode_tag_for_field(stream, field))
		return false;

	return pb_encode_string(stream, (uint8_t*)str, strlen(str));
}

// 注意解码decode的最后一个参数是 void *arg
bool string_decode_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	uint8_t buffer[1024] = { 0 };

	/* We could read block-by-block to avoid the large buffer... */
	if (stream->bytes_left > sizeof(buffer) - 1)
		return false;

	int nLen = stream->bytes_left;
	// 读取数据
	if (!pb_read(stream, buffer, nLen))
		return false;

	/* Print the string, in format comparable with protoc --decode.
	* Format comes from the arg defined in main().
	*/
	//printf((char*)*arg, buffer);
	//strcpy((char*)*arg, buffer);
	memcpy((char*)*arg, buffer, nLen);
	return true;
}
/////////////////////////////////////////////////////////////////////
bool bytes_encode_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	pb_bytes_array_t* str = (pb_bytes_array_t*)*arg;

	//char *str = "Hello world!";

	if (!pb_encode_tag_for_field(stream, field))
		return false;

	// pb_encode_string 也可以BYTE数组
	return pb_encode_string(stream, (uint8_t*)str, str->size);
}

// 注意解码decode的最后一个参数是 void *arg
// 这里是解析一个还是多个？
bool bytes_decode_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	//pb_bytes_array_t byteArr[10] = { 0 };
	PB_BYTES_ARRAY_T(6) byteArr = { 0 };

	if (stream->bytes_left > sizeof(byteArr) - 1)
			return false;

	int nSize = stream->bytes_left;
	if (!pb_read(stream, &byteArr, stream->bytes_left))
		return false;
	memcpy((pb_bytes_array_t*)*arg,&byteArr, nSize);
	return true;
}
/////////////////////////////////////////////////////////////////////
// 保存动态的数组，这里是uint32数组，有10个数据
// 注意数组要循环保存
// 注意bool、enum、int32、int64、uint32和uint64类型全部使用pb_encode_varint保存
bool write_repeated_list_encode_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg)
{
	uint32_t* mydata = (uint32_t *)*arg;

	for (int i = 0; i < 10;i++)
	{
		pb_encode_tag_for_field(stream, field);
		uint64_t value = *(mydata+i);
		if (!pb_encode_varint(stream, value))
			return false;
	}
	return true;
}

// 读取uint32数组
bool list_decode_callback(pb_istream_t *stream, const pb_field_t *field, void **arg)
{
	uint64_t value;
	if (!pb_decode_varint(stream, &value))
		return false;

	*(uint32_t*)*arg = value; // 给当前值复制
	((uint32_t*)*arg)++;  // 注意这里数值移动到下一位,注意因为是uint32_t数组，对应uint32_t*转换
	return true;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

int SendData(uint8_t * buffer,size_t max_buf_size, size_t *message_length)
{
	bool status;
	AllValue message = AllValue_init_zero;
	// 创建输出流用于写入
	pb_ostream_t stream = pb_ostream_from_buffer(buffer, max_buf_size);

	// 填充数据
	message.myfloat = 0.123456789;
	message.myint32 = 123456789; // 9 位
	message.myint64 = 1234567890123456789; //19位
	message.myuint32 = 400004000;
	message.myuint64 = 10002000300040005000; //20位
	message.mysint32 = 1234567;
	message.mysin64 = 12345678901234567;
	message.myfixed32 = 1234;
	message.myfixed64 = 1234567890;
	message.mysfixed32 = 323232;
	message.myfixed64 = 64646464;
	message.mybool = 1;

	char*str = "Hello,你好！";
	message.mystring.arg = str;
	message.mystring.funcs.encode = &string_encode_callback;

	PB_BYTES_ARRAY_T(6) bytes = { 6, { '1', '2', '3', '4', '5' } };
	message.mybytes.arg = &bytes;
	message.mybytes.funcs.encode = &bytes_encode_callback;

	message.myAge = 18;

	// uint32 list
	uint32_t  datalist[10] = { 0 };
	for (int i = 0; i < 10; i++)
	{
		datalist[i] = 0xEE00 + i;
	}
	message.myList.arg = datalist;
	message.myList.funcs.encode = write_repeated_list_encode_callback;

	status = pb_encode(&stream, AllValue_fields, &message);
	// 获取写入的长度
	*message_length = stream.bytes_written;
	if (!status)
	{
		printf("Encoding failed: %s\n", PB_GET_ERROR(&stream));
		return 0;
	}
	return 1;
}

int RecvData(uint8_t * recv_data, size_t message_length)
{
	bool status;
	AllValue message = AllValue_init_zero;

	// 定义string的解码方式
	char buf[128] = { 0 };
	message.mystring.arg = buf;
	message.mystring.funcs.decode = string_decode_callback;

	// 接收bytes数组
	PB_BYTES_ARRAY_T(100) bytelist;
	memset(bytelist.bytes, 0, 100);
	message.mybytes.arg = &bytelist;
	message.mybytes.funcs.decode = &bytes_decode_callback;

	// 接收list
	uint32_t  datalist[100] = { 0 };
	message.myList.arg = datalist;
	message.myList.funcs.decode = list_decode_callback;

	// 接收
	pb_istream_t stream = pb_istream_from_buffer(recv_data, message_length);

	status = pb_decode(&stream, AllValue_fields, &message);
	if (!status)
	{
		printf("Decoding failed: %s\n", PB_GET_ERROR(&stream));
		return 0;
	}
	printf("Print buf = %s\n", buf);
	printf("Byte buf = %s\n", bytelist.bytes);
	
	printf("repeated uint32 myList:\n");
	for (int i = 0; i < 10;i++)
	{
		printf("%08x\n", datalist[i]);
	}
	return 1;
}
/////////////////////////////////////////////////////////////////////
#define BUFF_SIZE 256
int main(int agrc, char** agrv)
{
	uint8_t  buffer[BUFF_SIZE] = { 0 }; // 用于流的保存
	bool status;
	size_t message_length;
	// 编码消息
	status = SendData(buffer, BUFF_SIZE,&message_length);
	if (status)
	{
		printf("\n序列化后的数据,大小%d 内容：\n", message_length);
		printf("%s\n", buffer);

		printf("\n解码测试\n");
		// 接受消息
		RecvData(buffer, message_length);
	}
	return 0;
}