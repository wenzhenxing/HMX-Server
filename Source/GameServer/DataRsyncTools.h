#ifndef __DATA_RSYNC_TOOLS_H_
#define __DATA_RSYNC_TOOLS_H_

#include "DataRsyncStruct.h"

struct NetMsgSS;
class zSession;

/*-------------------------------------------------------------------
 * @Brief : ��ʹ�������ݵ����ݿ⣬��ʱ��ѯ���ݿ⣬���������ݣ������
 *          �����������ҿ������߻����߻��ڱ���������
 *			��Ҫ�����������ݣ�����ʱһ������ߴ������ɣ�
 *			�磺��ֵ���棬��������������ʼ������������
 *			scene->dp->database,dp->database->dp->scene
 * @Author:hzd 2015:12:11
 *------------------------------------------------------------------*/

/*-------------------------------------------------------------------
 * @Brief :������ı�Ϊָ�����͹��ñ����絥һֵ������������ֵ������
 * 
 * @Author:hzd 2015:12:21
 *------------------------------------------------------------------*/


class DataRsyncTools : public Single<DataRsyncTools>
{
public:

	DataRsyncTools(zSession* pDbServer);

	void SavaMixItemNumber(EMixItemNumberType eType,int64 nValue);

	void SaveMixItemBinary(EMixItemBinaryType eType,const char* data,int32 nLen);

	int64 GetMixNumber(EMixItemNumberType eType);

	void GetMixNumber(EMixItemBinaryType eType,char* o_data);

private:

	zSession* m_pDbServer;

};

#endif

