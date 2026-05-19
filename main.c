#include <stdio.h>
#include <pthread.h>
#define MAX_CARS 100

void* Car(void* arg){
    pthread_t thread_id;
    int car_num;
    int *direction = (int*)arg;

    
}


int main(int argc, char const *argv[])
{
    pthread_t cars[MAX_CARS];
    int num_cars = atoi(argv[1]);

    if (argc !=2){
        printf("Użycie: /Bridge <liczba samochodów> ");
        exit(-1);
    }
    if (num_cars < 1 || num_cars > MAX_CARS){
        printf("Liczba samochodów musi być z zakresu 1-%d", MAX_CARS);
        exit(-1);
    }

    int direction; // 0 - left, 1 - right
    for (size_t i = 0; i < num_cars; i++) //create threads for cars
    {
        direction = rand() % 2;
        pthread_create(&cars[i], NULL, Car, &direction);
    }

    for (size_t i = 0; i < num_cars; i++) //wait for all cars to finish
    {
        pthread_join(cars[i], NULL);
    }
    
    return 0;
}
