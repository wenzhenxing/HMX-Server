#ifndef __STATISTIC_H_
#define __STATISTIC_H_

#include "Includes.h"

/*
 *
 *	Detail: �ռ���Ϣ
 *   
 *  Created by hzd 2015/06/19  
 *
 */
class Statistic : public Single<Statistic>
{
public:
	Statistic(void);
	~Statistic(void);

	void Update(int32 nSrvTime);

};


#endif



