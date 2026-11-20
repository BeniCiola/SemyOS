bool strcmp(char *str1, char *str2){
	if(str1 == NULL || str2 == NULL) return false;

	for(uint32_t i = 0; str1[i] != 0; i++){
		if(str1[i] != str2[i]) return false;
	}

	return true;
}

uint32_t strsize(char *str){
	if(str == NULL) return 0;

	uint32_t i = 0;

	while(str[i] != 0) i++;

	return (i + 1);
}

void strlower(char *str){
	if(str == NULL) return;

	for(int i = 0; str[i] != 0; i++){
        if(str[i] >= 'A' && str[i] <= 'Z'){
            str[i] += ('a' - 'A');
        }
    }
}

void strupper(char *str){
	if(str == NULL) return;

	for(int i = 0; str[i] != '\0'; i++){
        if(str[i] >= 'A' && str[i] <= 'Z'){
            str[i] += ('a' - 'A');
        }
    }
}
