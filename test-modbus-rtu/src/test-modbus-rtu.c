/*
 ============================================================================
 Name        : test-modbus-rtu.c
 Author      : juan
 Version     :

 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>


#include <sys/timerfd.h>
#include <sys/time.h>

#include "modbus-rtu-static.h"

/*
 * pasa a flotante los 2 registros de 16 bits que devuelve libmodbus.
 */
float pasar_4_bytes_a_float_2(unsigned char * buffer) {
	unsigned char tmp[4];

	tmp[3] = buffer[1];
	tmp[2] = buffer[0];
	tmp[1] = buffer[3];
	tmp[0] = buffer[2];

	//return *(float *) (tmp);   da el warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]
	//return *(float *) (unsigned long *) (tmp);da el warning: dereferencing type-punned pointer will break strict-aliasing rules [-Wstrict-aliasing]

	float *pvalor;
	pvalor=(float *)tmp;
	return *pvalor;
}

char * get_iso_time(){
	time_t tiempo;
	tiempo=time(0);
	static char buf[sizeof("2019-01-01T09:00:00+00:00")];
	strftime(buf, sizeof buf, "%FT%T%z", localtime(&tiempo));
	return (&buf[0]);
}

/*
 * Obtiene el siguiente segundo sobre el tiempo real desde el momento actual
 */
void siguiente_segundo (struct timeval *tv_siguiente_segundo){
	struct timeval tv_actual;
	struct tm *ptiempo_bd, tiempo_bd;
	/* obtiene el tiempo actual */
	gettimeofday(&tv_actual, NULL);
	/* Descompone el tiempo y obtiene siguiente segundo*/
	ptiempo_bd=localtime(&tv_actual.tv_sec);
	tiempo_bd=*ptiempo_bd;
	tiempo_bd.tm_sec+=1;
	tv_siguiente_segundo->tv_sec=mktime(&tiempo_bd);
	tv_siguiente_segundo->tv_usec=0;
}

int init_espera_siguiente_segundo(){
	/*<string.h>
		 * Temporizador de cada segundo
		 */
		int fd_timer_segundo; //
		fd_timer_segundo = timerfd_create(CLOCK_REALTIME, 0);
		if(fd_timer_segundo==-1) {
			return -1;
		}
		struct timeval tiempo;
		struct itimerspec ts;

		/*
		 * alinea el inicio del temporizador con el inicio de segundo de tiempo real
		 */
		siguiente_segundo(&tiempo);

		ts.it_value.tv_sec=tiempo.tv_sec;
		ts.it_value.tv_nsec=0;
		ts.it_interval.tv_sec=0;
		ts.it_interval.tv_nsec=0;
		if (timerfd_settime(fd_timer_segundo, TFD_TIMER_ABSTIME, &ts, NULL) == -1){
			return -1;
		}

		/*
		 * reajuste de temporizador periodico a 1 segundo
		 */
		struct itimerspec timer = {
		        .it_interval = {1, 0},  /* segundos y nanosegundos  */
		        .it_value    = {1, 0},
		};

		if (timerfd_settime(fd_timer_segundo, 0, &timer, NULL) == -1){
			close(fd_timer_segundo);
			return -1;
		}

		return fd_timer_segundo;
};

int espera_siguiente_segundo(int fd_timer_segundo){
	uint64_t s;
	uint64_t num_expiraciones;

	s = read(fd_timer_segundo, &num_expiraciones, sizeof(num_expiraciones)); /* esta funcion debe leer 8 bytes del fichero temporizador por eso se declara como uint64_t*/
	if (s != sizeof(num_expiraciones)) {
		return -1;
	}
	return (int)(num_expiraciones - 1);
}

int set_baudrate(modbus_t *ctx, int baud_rate){
	struct termios opciones_serial;

	struct _modbus {
	    /* Slave address */
	    int slave;
	    /* Socket or file descriptor */
	    int s;
#if 0
	    int debug;
	    int error_recovery;
	    struct timeval response_timeout;
	    struct timeval byte_timeout;
	    struct timeval indication_timeout;
	    void *backend;
	    void *backend_data;
#endif
	};
	struct _modbus *puntero;

	puntero=(struct _modbus *)ctx;

	int fd;
	fd= puntero->s;

	if (tcgetattr(fd, &opciones_serial) < 0) {

		return -1;
	}
	/*
	* Ajustes de la configuracion serie
	*/
	switch (baud_rate){
			case 1200:
				cfsetspeed(&opciones_serial,B1200);
				break;
			case 2400:
				cfsetspeed(&opciones_serial,B2400);
				break;
			case 4800:
				cfsetspeed(&opciones_serial,B4800);
				break;
			case 9600:
				cfsetspeed(&opciones_serial,B9600);
				break;
			default:
				cfsetspeed(&opciones_serial,B2400);
	}

	// Aplica la configuración
	if (tcsetattr(fd, TCSANOW, &opciones_serial) < 0) {
		return -1;
	};
	return 0;
}


int main(void) {

	fprintf(stderr, "test-modbus-rtu v 0.35\n"
			"Emplea libreria estatica libmodbus adaptada solo para RTU\n"
			"prueba con dos slaves a diferente velocidad\n");
	int rc;
	modbus_t *ctx;
	ctx = modbus_new_rtu("/dev/ttyUSB0", 2400, 'N', 8, 1);
	if (!ctx) {
		fprintf(stderr, "Failed to create the context: %s\n",
		modbus_strerror(errno));
		exit(1);
	}
	// abre fichero del puerto serie
	if (modbus_connect(ctx) == -1) {
		fprintf(stderr, "Unable to connect: %s\n", modbus_strerror(errno));
		modbus_free(ctx);
		exit(1);
	}

#if 1
	modbus_t *ctx2;
	ctx2 = modbus_new_rtu("/dev/ttyUSB0", 2400, 'N', 8, 1);
	if (!ctx2) {
		fprintf(stderr, "Failed to create the context: %s\n",
		modbus_strerror(errno));
		exit(1);
	}
#endif

#if 1
	if (modbus_connect(ctx2) == -1) {
		fprintf(stderr, "Unable to connect: %s\n", modbus_strerror(errno));
		modbus_free(ctx2);
		exit(1);
	}
#endif

	//Set the Modbus address of the remote slave
	rc=modbus_set_slave(ctx, 1);
	if (rc == -1) {
	    fprintf(stderr, "Invalid slave ID\n");
	    modbus_free(ctx);
	    return -1;
	}

#if 1
	rc=modbus_set_slave(ctx2, 1);
	modbus_set_slave(ctx2, 1);
	if (rc == -1) {
	    fprintf(stderr, "Invalid slave ID\n");
	    modbus_free(ctx2);
	    return -1;
	}
#endif

	uint16_t reg[128]; // will store read registers values

	//Read 32 holding registers starting from address 0
	int reg_solicitados = 32;
	int num;

	int fd_timer_segundo;
	fd_timer_segundo=init_espera_siguiente_segundo();
	if(fd_timer_segundo<0){
		fprintf(stderr, "Error en temporizador errno:%d", errno);
	}

	int num_faltas=0;
	int faltas_acumuladas=0;
	int errores=0;
	int lecturas=0;


	for (;;) {

		num_faltas=espera_siguiente_segundo(fd_timer_segundo);
		faltas_acumuladas+=num_faltas;
		memset(reg, 0, sizeof(reg));

		char * tiempo;
		tiempo=get_iso_time();

		rc=modbus_set_slave(ctx, 1);
			if (rc == -1) {
			    fprintf(stderr, "Invalid slave ID\n");
			    modbus_free(ctx);
			    return -1;
		}
		set_baudrate(ctx,2400);
		//Read holding registers starting from address 0x00 (tension)
		num = modbus_read_input_registers(ctx, 0x00, reg_solicitados, reg);
		if (num != reg_solicitados) { // number of read registers is not the one expected
			fprintf(stderr, "%s Failed to read: %s\n", get_iso_time(), modbus_strerror(errno));
			errores++;
		}
		lecturas++;

		printf("%s ", tiempo);
		printf("faltas_acum: %d "   , faltas_acumuladas );
		printf("Tension: %03.0f  "   , pasar_4_bytes_a_float_2((unsigned char *) &reg[0]));
		printf("Intensidad: %02.1f  ", pasar_4_bytes_a_float_2((unsigned char *) &reg[0x06]));

		rc=modbus_set_slave(ctx2, 1);
		if (rc == -1) {
			    fprintf(stderr, "Invalid slave ID\n");
			    modbus_free(ctx2);
			    return -1;
		}
		set_baudrate(ctx2,2400);
		//Read holding registers starting from address 0x46 (frecuencia)
		memset(reg, 0, sizeof(reg));
		num = modbus_read_input_registers(ctx2, 0x46, reg_solicitados, reg);
		if (num != reg_solicitados) { // number of read registers is not the one expected
			fprintf(stderr, "%s Failed to read: %s\n", get_iso_time(), modbus_strerror(errno));
			errores++;
		}
		lecturas++;

		printf("Frecuencia: %02.2f  "  , pasar_4_bytes_a_float_2((unsigned char *) &reg[0]));
		printf("Energia Imp:: %06.2f  ", pasar_4_bytes_a_float_2((unsigned char *) &reg[0x48-0x46]));

		fflush(stdout);

		printf("tasaerror: %06.2f  ", errores/(float)lecturas );
		printf("\r");
	}

	modbus_close(ctx);
	modbus_free(ctx);
#if 1
	modbus_close(ctx2);
#endif
#if 1
	modbus_free(ctx2);
#endif

	return EXIT_SUCCESS;

}
