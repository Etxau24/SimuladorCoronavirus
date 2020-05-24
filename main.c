#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "persona.h"
#include "probabilidad.h"

#define SEED 0

/* Seguramente haya que usar MPI Isend y MPI_Irecv con el parametro en recv MPI_ANY_SOURCE para escuchar a todos los nodos
de manera no bloqueante.

*/


// CALCULAR LA MEDIA DE EDAD
// (Par: struct persona, int poblacion actual)
int mediaEdad(struct persona *per, int pobl){
	int i;
	int media = 0;
	for(i=0; i<pobl; i++){
		media += per[i].edad;
	}
	return (media/pobl);
}

// FUNCION DE PROGRAMA PRINCIPAL
int main(int argc, char** argv) {
	if(argc!=9) {
		fprintf(stderr,"Funcionamiento: %s <tiempoASimular> <tamanoAncho> <tamanoAlto> <radio> <probRadio> <poblacion> <edadMedia> <batch>\n", argv[0]);
		exit(1);
	}

	int TIEMPO 		= atoi(argv[1]);
	int ESCHEIGHT 	= atoi(argv[2]);
	int ESCWIDTH 	= atoi(argv[3]);
	int RADIO 		= atoi(argv[4]);
	float PROBRADIO = atof(argv[5]);
	int POBLACION 	= atoi(argv[6]);
	int EDADMEDIA 	= atoi(argv[7]);
	int BATX 		= atoi(argv[8]);

	if (PROBRADIO > 0.9 || PROBRADIO < 0 || TIEMPO < BATX || TIEMPO < 1 || RADIO >= ESCWIDTH || RADIO >= ESCHEIGHT) {
        fprintf(stderr,"Error de parámetros: \n\t- La probabilidad de contagio debe estar comprendido entre 0 y 1.\n\t- El tiempo a simular debe ser mayor que 1.\n\t- El batch no puede ser mayor que el tiempo a simular.\n\t- El radio de contagio debe ser menor que el tamaino del lienzo.\n");
		exit(1);
	}

	// INICIALIZACIONES MPI
	int world_rank, world_size;
	MPI_Init(NULL, NULL);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	// INICIALIZACIONES VARIABLES
	if(world_rank == 0)
		printf("STATUS: Inicializando variables...\n");
	int i, e, j;
   	int rangox, rangoy;
	int muertosRonda, curadosRonda, contagiadosRonda;
	int muertosNodo, curadosNodo, contagiadosNodo;
	int muertosTotales = 0;
	int curadosTotales = 0;
	int contagiadosTotales = 0;
	int diasTranscurridos = 0;
	int desv = 0;
	int pobActual = POBLACION;
	int edadMedia = EDADMEDIA;
	char str1[24];
	char str2[24];
	clock_t inicio, fin;

	//PARA MPI
 	int *proc; //Con este puntero guardaremos los nodos que van a trabajar.
	int nfilas;//Se guarda la cantidad de filas que tiene que calcular cada nodo.
	int restofilas;//Luego se repartiran las que sobren en caso de que las dimensiones no sean multiplo.
	int ncolumnas;//Se guarda la cantidad de columnas que tiene que calcular cada nodo.
	int restocolumnas;//Luego se repartiran las que sobren en caso de que las dimensiones no sean multiplo.

	int *coordenadasX; //Aqui guardaremos las coordenadas X de las que es responsable cada nodo.
	int *coordenadasY; //Aqui guardaremos las coordenadas Y de las que es responsable cada nodo.

	// INICIO DE TIEMPO DE EJECUCION
	if(world_rank == 0)
		inicio = clock();

	// 
	if(world_size<=ESCHEIGHT && world_size<=ESCWIDTH){ //Si hay mas filas que nodos y mas columnas que nodos, entonces el comunicador contendrá a todos.
			proc=malloc(world_size*sizeof(int));
			coordenadasX=malloc(world_size*sizeof(int));
			coordenadasY=malloc(world_size*sizeof(int));

			nfilas=ESCHEIGHT/world_size; //Ver cuantas filas tienen que controlar cada nodo.
			restofilas=ESCHEIGHT%world_size; //Las que sobran.
			ncolumnas=ESCWIDTH/world_size; //Vemos cuantas columnas tienen que controlar cada nodo.
			restocolumnas=ESCWIDTH%world_size; //Las columnas que sobran.

			for(i=0;i<world_size;i++){
					proc[i]=i;
					//EJE X (Para calcular las cordenadas) y saber a quien hay que enviarle cada persona.
					coordenadasX[i]=h*nfilas; //Hay que controlar los ultimos elementos TODO
					//EJE Y (Para calcular las cordenadas) y saber a quien hay que enviarle cada persona.
					coordenadasY[h]=h*ncolumnas; //Hay que controlar los ultimos elementos TODO
			}

	}else{ //Si no hay tantas filas, solo utilizar algunos nodos.
			proc=malloc(ESCHEIGHT*sizeof(int));
			for(i=0;i<=tamano;i++){
					proc[i]=i;
			}
			nfilas=1;
			restofilas=0;
	}

	desv = calculo_desv(EDADMEDIA);
	srand(SEED);

	// INICIALIZACION FICHEROS (MPI)
	int posic;
	MPI_File p;
	MPI_Status statPosic;
	posic = MPI_File_open( MPI_COMM_WORLD, "historialposic.txt", MPI_MODE_RDWR | MPI_MODE_CREATE, MPI_INFO_NULL, &p);
	int dias;
	MPI_File d;
	MPI_Status statDias;
	dias = MPI_File_open( MPI_COMM_WORLD, "historialdias.txt", MPI_MODE_RDWR | MPI_MODE_CREATE, MPI_INFO_NULL, &d);

	// INICIALIZACION FICHEROS (SERIE)
	// FILE *dias, *posic;
	// posic = fopen("historialposic.txt","w+");
	// dias  = fopen("historialdias.txt", "w+");

	// INICIALIZACION ARRAY PERSONAS
	struct persona *personas;
	personas = malloc(POBLACION*sizeof(struct persona));

	// IMPRESION DE VARIABLES INTRODUCIDAS POR PARAMETRO
	if(world_rank == 0)
		printf("STATUS: DATOS INTRODUCIDOS: \n\tTIEMPO %d\n\tPOBLACION: %d\n\tESCENARIO: %dx%d\n\tRADIO CONTAGIO: %d  PROB DE CONTAGIO RADIO: %.2f\n",
			TIEMPO, POBLACION, ESCHEIGHT, ESCWIDTH, RADIO, PROBRADIO);

	// El primer nodo genera toda la poblacion.
	if(world_rank == 0){
		printf("STATUS: Creando población...\n");
		for(i=0; i<POBLACION; i++)
			personas[i] = crearPersona(EDADMEDIA, ESCWIDTH, ESCHEIGHT,desv);
	}

	// PRIMER INFECTADO!
	if(world_rank == 0){
		printf("STATUS: PRIMER INFECTADO!\n");
		int aux = rand()%POBLACION;
		personas[aux].estado = 1;
		contagiadosTotales++;
	}

	//Ahora habria que compartir toda la informacion con los demas nodos. Scatter?
	// Si, y el "aux" anterior hay que compartir tambien con los demas, pa que sepan cual esta infectau

	// BUCLE PRINCIPAL
	if(world_rank == 0)
		printf("STATUS: Iniciando programa...\n");
	while(diasTranscurridos < TIEMPO) {
		muertosRonda = 0;
		curadosRonda = 0;
		contagiadosRonda = 0;

		// MOVER PERSONA y CAMBIAR VELOCIDAD PARA LA SIGUIENTE RONDA
		for(i=0; i<pobActual; i++){
			//AQUI HABRA QUE VER QUIEN LO TIENE QUE CALCULAR.
			moverPersona(&personas[i], ESCWIDTH, ESCHEIGHT);
			// FICHERO: GUARDAR CAMBIO DE PERSONA
			if(diasTranscurridos%BATX==0)
				// ESCRIBIR EN FICHERO CON MPI
				sprintf(str1, "%d,%d,%d:", personas[i].pos[0], personas[i].pos[1], personas[i].estado);
				// AQUI FALTA EL OFFSET
				MPI_File_seek(p, MPI_Offset offset, MPI_SEEK_END);
				MPI_File_write(p, str1, sizeof(str1+3), MPI_CHAR, &statPosic);
				// fprintf(posic,"%d,%d,%d:",personas[i].pos[0],personas[i].pos[1],personas[i].estado);
        	}

		// BARRERA
		MPI_Barrier(MPI_COMM_WORLD);

		// FICHERO: SALTAR DE LINEA TRAS MOVER TODAS LAS PERSONAS
		if(diasTranscurridos%BATX==0)
			// ESCRIBIR EN FICHERO CON MPI
			// PONER OFFSET AL FINAL
			// MPI_File_seek(MPI_File fh, MPI_Offset offset, int whence);
			MPI_File_write(p, '\n', 1, MPI_CHAR, &statPosic);
			// fprintf(posic,"\n");

	    	// INFECTADOS: COMPROBAR RADIO DE CONTAGIOS y DECISIONES DE MUERTE o SUPERVIVENCIA
        	for(i=0; i<pobActual; i++){
				//AQUI HABRA QUE VER QUIEN LO TIENE QUE CALCULAR.
				/* Algo del estilo:
				if(personas[i].pos[0]>=coordenadasX[world_rank]&&personas[i].pos[0]<(coordenadasX[world_rank]+nfilas)){
					if(personas[i].pos[1]>=coordenadasY[world_rank]&&personas[i].pos[1]<(coordenadasY[world_rank]+ncolumnas)){
						//ESTO ES LO DE MAS ABAJO.
						if(personas[i].estado == 1 || personas[i].estado == 2){
						rangox = personas[i].pos[0];
						rangoy = personas[i].pos[1];

						// DECIDIR SI SE CONTAGIA CADA INDIVIDUO EN BASE AL RADIO DE UN CONTAGIADO
						for(e=0; e<pobActual; e++)
							contagiadosRonda += infecPersona(&personas[e], rangox, rangoy, RADIO, PROBRADIO);

						// DECIDIR SI SE MUERE O SE RECUPERA
						int samatao = matarPersona(&personas[i]);
						if(samatao == 0){				// SE MUERE
							for(e=i; e<pobActual-1; e++)
								personas[e] = personas[e+1];
							muertosRonda++;
							contagiadosTotales--;
							pobActual--;
						} else if(samatao == 2){			// SE CURA
							curadosRonda++;
							contagiadosTotales--;
						}
					}
					}
				}

				*/
			if(personas[i].estado == 1 || personas[i].estado == 2){
				rangox = personas[i].pos[0];
				rangoy = personas[i].pos[1];

				// DECIDIR SI SE CONTAGIA CADA INDIVIDUO EN BASE AL RADIO DE UN CONTAGIADO
				for(e=0; e<pobActual; e++)
					contagiadosRonda += infecPersona(&personas[e], rangox, rangoy, RADIO, PROBRADIO);

				// DECIDIR SI SE MUERE O SE RECUPERA
				int samatao = matarPersona(&personas[i]);
				if(samatao == 0){				// SE MUERE
					for(e=i; e<pobActual-1; e++)
						personas[e] = personas[e+1];
					muertosRonda++;
					contagiadosTotales--;
					pobActual--;
				} else if(samatao == 2){			// SE CURA
					curadosRonda++;
					contagiadosTotales--;
				}
			}
		}

		// FUNCION MPI: RECOGER VALORES DE NODOS Y SUMAR
		// contagiadosRonda = 0;
		// MPI_Reduce(&contagiadosNodo, &contagiadosRonda, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
		// curadosRonda = 0;
		// MPI_Reduce(&curadosNodo, &curadosRonda, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);
		// muertosRonda = 0;
		// MPI_Reduce(&muertosNodo, &muertososRonda, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

		// ACTUALIZAR EDAD MEDIA
		edadMedia = mediaEdad(personas, pobActual);

		// ACTUALIZAR VALORES TOTALES
		contagiadosTotales += contagiadosRonda;
		curadosTotales += curadosRonda;
		muertosTotales += muertosRonda;

		// RULAR TIEMPO
		diasTranscurridos++;

		// VISUALIZAR PROGRESO
		if(world_rank == 0)
			if(diasTranscurridos%BATX==0){//Si es multiplo de lo metido significa que se va a guardar en el fichero los datos con el formato establecido
				// ESCRIBIR EN FICHERO CON MPI
				sprintf(str2, "%d:%d,%d,%d\n", diasTranscurridos, contagiadosTotales, curadosTotales, muertosTotales);
				// MPI_File_seek(MPI_File fh, MPI_Offset offset, int whence);
				MPI_File_write(d, str2, sizeof(str2)+4, MPI_CHAR, &statDias);
				// fprintf(dias, "%d:%d,%d,%d\n", diasTranscurridos,contagiadosTotales,curadosTotales,muertosTotales);
				printf("DIA %i: %i INFECTADOS (%i NUEVOS), %i RECUPERADOS (%i NUEVOS), %i FALLECIDOS (%i NUEVOS). POBLACION: %i, EDAD MEDIA: %i\n", diasTranscurridos, contagiadosTotales, contagiadosRonda, curadosTotales, curadosRonda, muertosTotales, muertosRonda, pobActual, edadMedia);
			}

		// CONTROLAR SI SE DEBE FINALIZAR EL PROGRAMA
       		if(contagiadosTotales == 0) break;
       		if(pobActual == 0) break;
	}

	if(world_rank == 0){
		printf("DIA %i: %i INFECTADOS (%i NUEVOS), %i RECUPERADOS (%i NUEVOS), %i FALLECIDOS (%i NUEVOS). POBLACION: %i, EDAD MEDIA: %i\n", diasTranscurridos, contagiadosTotales, contagiadosRonda, curadosTotales, curadosRonda, muertosTotales, muertosRonda, pobActual, edadMedia);

		// FIN DE TIEMPO DE EJECUCION
		fin = clock();
		double tiempoTotal = (double)(inicio - fin) / CLOCKS_PER_SEC;

		// LIBERAR MEMORIA, CERRAR ARCHIVOS y CERRAR MPI AL ACABAR PROGRAMA
		printf("STATUS: Liberando memoria alocada...\n");
		printf("STATUS: Tiempo de ejecución: %.2f\n", tiempoTotal);
		printf("STATUS: Fin del programa.\n");
	}

	free(personas);
	// CERRAR ARCHIVOS MPI
	MPI_File_close(&p);
	MPI_File_close(&d);
	// fclose(dias);
	// fclose(posic);
	MPI_Finalize();
}
