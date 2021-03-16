#ifndef BYTEORDER_H_INCLUDED
#define BYTEORDER_H_INCLUDED

#include <stdint.h>

#define SYS_BIG_ENDIAN (!*((uint8_t*)&((uint16_t){1}))))

public void reverseBytes(uint8_t* ptr, int nbytes){
	
	if(nbytes < 2) return;
	
	uint8_t temp = 0;
	int i = 0;
	int j = nbytes-1;
	
	while(j > i){
		temp = *(ptr+i);
		*(ptr+i) = *(ptr+j);
		*(ptr+j) = temp;
		i++; j--;
	}
	
}

#endif //BYTEORDER_H_INCLUDED