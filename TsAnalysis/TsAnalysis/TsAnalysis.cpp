#include <iostream>
using namespace std;

// 全局变量
int pktLength = 0;	// TS包长度（188或204）
int firstPktPos = 0;	//第一个TS包的起始位置

// 结构体
struct tsPacketHeader
{
	int ts_pkt_num;

	int sync_byte = 0;				// 同步字节 - 8 bit（TS包头的第1字节）：固定为0x47
	int transport_error_indicator = 0;		// 传输错误指示 - 1 bit（TS包头的第2字节起始）
	int payload_unit_start_indicator = 0;	// 开始指示 - 1 bit
	int transport_priority = 0;				// 传输优先级 - 1 bit
	long PID = 0;							// Packet identifier - 13 bit
	int transport_scrambling_control = 0;	// 加扰控制 - 2 bit（TS包头的第4字节起始）
	int adaptation_field_control = 0;		// 自适应域控制 - 2 bit：01或11表示含有有效载荷
	int continuity_counter = 0;	// 连续性计数器 - 4 bit
	int adaption_field_length = 0;			// 自适应域长度 - 8 bit（TS包头的第5字节）

	void PrintTsPktHdInfo()
	{
		cout << endl;
		cout << "TS packet NO. " << ts_pkt_num << ".\n";
	}
};

struct PatPacketHeader
{
	int ts_pkt_num;

	int table_id = 0;					// 8 bit（PAT包头的第1字节）：固定为0x00，表示该表为PAT表
	int section_syntax_indicator = 0;	// 1 bit（PAT包头的第2字节起始）：固定为1
	int section_length = 0;				// 12 bit：表示这个字节后面的有用字节数（包括CRC32）
	int transport_stream_id = 0;		// 16 bit（PAT包头的第4、5字节）：表示该传输流的ID
	int version_number = 0;				// 5 bit
	int current_next_indicator = 0;		// 1 bit
	int section_number = 0;				// 8 bit（PAT包头的第7字节）：PAT可能分为多段传输，表示分段号码（第一段为0）
	int last_section_number = 0;		// 8 bit（PAT包头的第8字节）：最后一个分段的号码
	void PrintPatHeaderInfo()
	{
		cout << endl;
		cout << "section_syntax_indicator: " << section_syntax_indicator << endl;
		cout << "section_length: " << section_length << endl;
		cout << "transport_stream_id: " << transport_stream_id << endl;
		cout << "version_number: " << version_number << endl;
		cout << "current_next_indicator: " << current_next_indicator << endl;
		cout << "section_number: " << section_number << endl;
		cout << "last_section_number: " << last_section_number << endl;
	}
};

// 函数声明
void ReadPatPkt(PatPacketHeader, unsigned char*, int);
void GetPmtPid(PatPacketHeader, unsigned char*, int);


int main(int argc, char* argv[])
{
	FILE* tsFilePtr = NULL;
	const char* tsFileName = argv[1];
	unsigned char* tempBuffer = new unsigned char[408];

	// 函数声明
	void FindSyncByte(unsigned char*);
	bool ReadTsPkt(unsigned char*, int);

	// 打开文件
	if (fopen_s(&tsFilePtr, tsFileName, "rb") == 0)
	{
		cout << "Successfully opened \"" << tsFileName << "\"." << endl;
	}
	else
	{
		cout << "Failed to open \"" << tsFileName << "\"." << endl;
		exit(0);
	}

	// 计算文件总字节数
	fseek(tsFilePtr, 0L, SEEK_END);
	int tsFileSize = ftell(tsFilePtr);
	rewind(tsFilePtr);
	cout << "The space that \"" << tsFileName << "\" accounts for is " << (float)tsFileSize / 1024 / 1024 << " MB." << endl << endl;

	// 查找sync_byte (0x47)
	fread(tempBuffer, sizeof(unsigned char), 408, tsFilePtr);
	FindSyncByte(tempBuffer);
	delete[]tempBuffer;

	unsigned char* singlePktBuffer = new unsigned char[pktLength];	// 存储单个TS包数据的缓冲区
	
	// 计算TS包的数量
	int pktCount = (tsFileSize - firstPktPos) / pktLength;
	cout << "There are " << pktCount << " TS Packets." << endl;

	// PAT包分析
	for (int i = 0; i < pktCount; i++)
	{
		fseek(tsFilePtr, firstPktPos + i * pktLength, SEEK_SET);	// 跳转到第i个TS包的起始点（i从0开始）
		fread(singlePktBuffer, sizeof(unsigned char), pktLength, tsFilePtr);	// 将第i个TS包读入缓冲区
		if (ReadTsPkt(singlePktBuffer, i) == 1)
		{
			break;
		}
	}

	delete[]singlePktBuffer;
	fclose(tsFilePtr);
}

void FindSyncByte(unsigned char* buffer_408)
{
	for (int i = 0;i < 204;i++)
	{
		//TS包长188字节的情况
		if (buffer_408[i] == 0x47 && buffer_408[i + 188] == 0x47)
		{
			pktLength = 188;
			firstPktPos = i;
			cout << "Packet length = " << 188 << ", and starts from " << i << "." << endl;
			break;
		}
		//TS包长204字节的情况
		else if (buffer_408[i] == 0x47 && buffer_408[i + 204] == 0x47)
		{
			pktLength = 204;
			firstPktPos = i;
			cout << "Packet length = " << 204 << ", and starts from " << i << "." << endl;
			break;
		}

	}
}

bool ReadTsPkt(unsigned char* PktBuff, int tsNum)	// 返回值为1表示成功读取到了PMT PID，为0表示没有读取到
{
	tsPacketHeader tsph;	// TS包头
	tsph.ts_pkt_num = tsNum;
	int offset = 0;
	tsph.sync_byte = PktBuff[0];
	tsph.transport_error_indicator = PktBuff[1] >> 7;
	tsph.payload_unit_start_indicator = (PktBuff[1] & 0x40) >> 6;
	tsph.transport_priority = (PktBuff[1] & 0x20) >> 5;
	tsph.PID = (int(PktBuff[1] & 0x1F) << 8) + PktBuff[2];	// 转换为int，防止溢出
	tsph.transport_scrambling_control = (PktBuff[3] & 0xc0) >> 6;
	tsph.adaptation_field_control = (PktBuff[3] & 0x30) >> 4;
	tsph.continuity_counter = PktBuff[3] & 0x0F;

	if (tsph.PID == 0)	// 若PID为0，则为PAT包，准备解析，否则退出
	{
		if ( (tsph.adaptation_field_control == 1) || (tsph.adaptation_field_control == 3) )	// 若不为01或11则无有效载荷，退出
		{
			//cout << "PAT exists." << endl;
			if (tsph.adaptation_field_control == 1)	
			{
				// 无适配域
				offset = 4;
			}
			else
			{
				// 有适配域
				tsph.adaption_field_length = PktBuff[4];
				offset = tsph.adaption_field_length + 5;
			}

			if (tsph.payload_unit_start_indicator == 1)
			{
				int pointer_field = PktBuff[offset];
				offset += pointer_field + 1;
			}

			// 开始读PAT表
			PatPacketHeader pph;	// PAT包头
			pph.ts_pkt_num = tsph.ts_pkt_num;
			ReadPatPkt(pph, PktBuff, offset);

			if (pph.table_id == 0)	//table_id为0则读PAT表
			{
				GetPmtPid(pph, PktBuff, offset);
				return 1;
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}
	else
	{
		//cout << "TS packet " << tsph.ts_pkt_num << " isn't a PAT packet.\n";
		return 0;
	}
}

void ReadPatPkt(PatPacketHeader pph, unsigned char* PktBuff, int offset)
{
	pph.table_id = PktBuff[offset];
	pph.section_syntax_indicator = (PktBuff[offset + 1] & 0x80) >> 7;
	//ph.section_length = (int(SgPktBuff[offset + 1] & 0x0F) << 8) + SgPktBuff[offset + 2];
	pph.transport_stream_id = PktBuff[offset + 3] << 8 + PktBuff[offset + 4];
	pph.version_number = (PktBuff[offset + 5] & 0x3E) >> 1;
	pph.current_next_indicator = PktBuff[offset + 5] & 0x01;
	pph.section_number = PktBuff[offset + 6];
	pph.last_section_number = PktBuff[offset + 7];

}

void GetPmtPid(PatPacketHeader pph, unsigned char* PktBuff, int offset)
{
	pph.section_length = (int(PktBuff[offset + 1] & 0x0F) << 8) + PktBuff[offset + 2];
	int totalPgmCount = (pph.section_length - 9) / 4;
	offset += 8;	// 偏移到N loop首字节
	cout << "\nTS packet " << pph.ts_pkt_num << " includes a PAT packet:\n";
	cout << "Programme\tPMT PID\n";
	for (int i = 0; i < totalPgmCount; i++)	// i表示Program number
	{
		int program_number = (PktBuff[offset] << 8) + PktBuff[offset + 1];
		int network_PID = ((PktBuff[offset + 2] & 0x1F) << 8) + PktBuff[offset + 3];	// PMT PID
		printf("%-9d\t%-5d\n", program_number, network_PID);
		offset += 4;	// 跳转到下一个program
	}
}