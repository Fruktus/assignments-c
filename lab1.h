#ifndef _LAB1_H
char **create_arr(int n); //alokuje tablice o dlugosci n zawierajaca wskazniki na chary
void remove_arr(char ***n, int size); //usuwa zaalokowana tablice dana wskaznikiem n

void fill_arr(char **a, int m); //wypelnia tablice a o dlugosci m pustymi blokami o dlugosci 100


void add_block(char **a, int m); //dodaje pusty blok do tablicy a pod index m, sprawdza czy jest wolny (jesli zajety, nie wstawia), nie zwraca informacji
void remove_block(char **a, int n); //usuwa blok spod n z tablicy a (zwalnia)

//currently tested \/
char *find_block(char **a, int n, int m); //szuka bloku w tablicy a o dlugosci m ktorego suma jest najblizej n
#endif
