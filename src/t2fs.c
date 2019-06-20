#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../include/t2fs.h"
#include "../include/apidisk.h"

#define ERRO -1
#define SUCESSO 0
#define MAXBLOCOS 256

int entradasPorDir; //Número máximo de entradas em um diretório
int initialized = 0; //Flag para saber se estruturas do sistema foram carregadas do disco
int entriesPerSector=0; //
int tamBloco;
int lastindex=0;
int entradasPorSet;

t_MBR mbr;
t_SUPERBLOCO superbloco;
DWORD* fat;
t_entradaDir* raiz;
t_entradaDir* diretorioAtual;
char caminhoAtual[MAX_FILE_NAME_SIZE+1];
unsigned int bitmap[MAXBLOCOS];




/*-----------------------------------------------------------------------------
Função:	Informa a identificação dos desenvolvedores do T2FS.
-----------------------------------------------------------------------------*/
int identify2 (char *name, int size) {
    if(strncpy(name, "Gabriel Barros de Paula - 240427 - Carine Bertagnolli Bathaglini- 274715 - Henrique da Silva Barboza  - 272730", size)){
        return SUCESSO;
    }
	return ERRO;
}

/*-----------------------------------------------------------------------------
Função:	Formata logicamente o disco virtual t2fs_disk.dat para o sistema de
		arquivos T2FS definido usando blocos de dados de tamanho 
		corresponde a um múltiplo de setores dados por sectors_per_block.
-----------------------------------------------------------------------------*/
int format2 (int sectors_per_block) {

	int i;

	BYTE sectorBuffer[SECTOR_SIZE];
	memset(sectorBuffer, '\0', SECTOR_SIZE);

	if(sectors_per_block < 4){
		printf("Numero de Setores muito baixo. Valor mínimo = 4 \n");
		return ERRO;
	}
	else if( (sectors_per_block % 2 ) != 0){
		printf("Um numero impar de setores p/ bloco não aproveita os setores do disco, tente um numero par.\n");
		return ERRO;
	}
	 
	 t_MBR mbr  = readsMBR(); // Lê dados do MBR para criar o superbloco

	 if(sectors_per_block > mbr.SetorFinalParticao1){
		 printf("Não há setores suficientes na partição do Disco\n");
		 return ERRO;
	 }

	 t_SUPERBLOCO superblock;

	 superblock.numBlocos = mbr.SetorFinalParticao1 / sectors_per_block;
	 superblock.setoresPorBloco = sectors_per_block;
	
	 for(i=0;i<8;i++){
		 superblock.bitmap[i] = 0;
	 }


	 //////CÁLCULO DO TAMANHO DA FAT :
	 int fatBytes = 4 * superblock.numBlocos;		// FAT vai ser unsigned int = 4 bytes
	 superblock.tamanhoFAT = fatBytes / SECTOR_SIZE;	//Tamanho da FAT em Setores

	 fat = malloc(SECTOR_SIZE * superblock.tamanhoFAT);

	 for(i=0;i<superblock.numBlocos;i++){
		 fat[i]=0;
	 }

	 writeFAT();



	 //No pior dos casos, FAT ocupa 8 setores (2 setores p/ bloco) e 2 blocos (TENTAR RESTRINGIR - Perguntar pro cechin)
	 //se não puder fazer IF, diretório raiz então começa a partir do suposto bloco 4 = Setor 7 (posição fixa)

	/*if(sectors_per_block==2){
		 superblock.blocoDirRaiz = 6;
	 }
	 else  */
	 
	 if(sectors_per_block==4){
		 superblock.blocoDirRaiz = 3;
	 }
	 else{
		 superblock.blocoDirRaiz = 2;
	 }
	
	memcpy(sectorBuffer, &superblock, SECTOR_SIZE);
	int teste = write_sector(SUPERBLOCKSECTOR, sectorBuffer);		//Grava Superbloco
	

	//GRAVAÇÃO DIRETÓRIO RAIZ
	//Cria um vetor de entrada de diretórios pra ser o diretório raiz
	//Marca todos como desocupado (filetype=0)
	//passa todos esses dados pra um vetorzão pra ser gravado
	//Grava diretório raiz:

	
	entradasPorDir = (SECTOR_SIZE*sectors_per_block) / sizeof(t_entradaDir); //Checar Conta
	
	t_entradaDir *entradas= malloc(SECTOR_SIZE*sectors_per_block);

	t_entradaDir vazio;
	vazio.fileType=FILETYPE_INV;
	memset(vazio.name,'\0',32);
	vazio.fileSize=0;
	vazio.firstBlock=0;

	//zera todas as outras entradas do diretório raiz

	for(i=1;i<entradasPorDir;i++){

		entradas[i]=vazio;
		
	}

	//Primeira entrada do diretório raiz contem as informações dele mesmo
	//Facilita operações futuras

	memset(entradas[0].name,'\0',32);
	char nome[] = ".\0";
	strcpy(entradas[0].name,nome);
	entradas[0].firstBlock = superblock.blocoDirRaiz;
	entradas[0].fileType = FILETYPE_DIR;
	entradas[0].fileSize = SECTOR_SIZE * sectors_per_block;


	/* BYTE writingBuffer[SECTOR_SIZE*sectors_per_block]; // Tamanho bloco em bytes = tamanhosetor*setoresPorBloco

	memcpy(writingBuffer,entradas,sizeof(entradas));*/

	int teste2 = writeBlock((BYTE*) entradas,superblock.blocoDirRaiz,superblock.blocoDirRaiz,sectors_per_block);

	if(teste ==1 && teste2 ==1){
		return SUCESSO;
	}
	else{
		return ERRO;
	}
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo arquivo no disco e abrí-lo,
		sendo, nesse último aspecto, equivalente a função open2.
		No entanto, diferentemente da open2, se filename referenciar um 
		arquivo já existente, o mesmo terá seu conteúdo removido e 
		assumirá um tamanho de zero bytes.
-----------------------------------------------------------------------------*/
FILE2 create2 (char *filename) {
	initializeEverything();
	int i=0;

	t_entradaDir* diretorio = getLastDirectory(filename);

	int handle = 0;
	char *nome = fileName(filename);

	//Se já existe:
	for(i=0;i<entradasPorDir;i++){
		if(strncmp(nome,diretorio[i].name,strlen(nome)) ==0 ){
			printf("Arquivo já existente\n");
			return ERRO;
		}
	}

	//Encontra local no diretorio pra colocar
	i=0;

	while(diretorio[i].fileType != FILETYPE_INV){
		i++;
	}

	if(i>entradasPorDir){
		printf("Diretório cheio\n");
		return ERRO;
	}

	//CRIAÇÃO DA ENTRADA DE DIRETÓRIO DO NOVO ARQUIVO
	t_entradaDir novaentrada;
	novaentrada.fileType = FILETYPE_ARQ;
	strcpy(novaentrada.name,nome);
	novaentrada.fileSize = SECTOR_SIZE * superbloco.setoresPorBloco;
	novaentrada.firstBlock = firstBitmapEmpty();

	fat[novaentrada.firstBlock] = FEOF;
	bitmap[novaentrada.firstBlock]=1;

	diretorio[i] = novaentrada;

	int teste = writeFAT();		//SEMPRE gravar FAT e Bitmap ao criar coisas novas
	updatesBitmap();

	if(teste != 0){
		printf("FAT não pode ser atualizada\n");
	}

	if(writeBlock((BYTE*) diretorio,diretorio[0].firstBlock,superbloco.blocoDirRaiz,superbloco.setoresPorBloco ) != SUCESSO ){
		printf("Erro ao gravar no disco\n");
		return ERRO;
	}

	handle = open2(filename);
	if(handle==ERRO){
		return ERRO;
	}

	printf("Arquivo criado corretamente!\n");
	return SUCESSO;


}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um arquivo do disco. 
-----------------------------------------------------------------------------*/
int delete2 (char *filename) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um arquivo existente no disco.
-----------------------------------------------------------------------------*/
FILE2 open2 (char *filename) {
	
	initializeEverything();

	int handle;

	if(opened_files==10){
		printf("Limite máximo de arquivos aberto\n");
		return ERRO;
	}

	t_entradaDir* entradaAberta;

	if( (entradaAberta = abrirArquivo(filename)) == NULL ){
		return ERRO;
	}

	if(entradaAberta->fileType != FILETYPE_ARQ){
		printf("Não é um arquivo\n");
		return ERRO;
	}
	//Pega diretório pai
	t_entradaDir *pai = getLastDirectory(filename);

	opened_files++;

	t_openedFile arquivoAberto;
	arquivoAberto.currentPoint = 0;
	arquivoAberto.registro=entradaAberta;
	arquivoAberto.blocoPai = pai[0].firstBlock;

	handle=freeHandle();

	openedFiles[handle] = arquivoAberto;
	filesMap[handle] = 1;

	return SUCESSO;

}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um arquivo.
-----------------------------------------------------------------------------*/
int close2 (FILE2 handle) {

	initializeEverything();

	if(handle > 0 && handle<MAX_OPENEDF){
		return -1;
	}
	if(filesMap[handle==0]){
		return-1;
	}
	filesMap[handle] = 0;
    opened_files--;

	return 0;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a leitura de uma certa quantidade
		de bytes (size) de um arquivo.
-----------------------------------------------------------------------------*/
int read2 (FILE2 handle, char *buffer, int size) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para realizar a escrita de uma certa quantidade
		de bytes (size) de  um arquivo.
-----------------------------------------------------------------------------*/
int write2 (FILE2 handle, char *buffer, int size) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para truncar um arquivo. Remove do arquivo 
		todos os bytes a partir da posição atual do contador de posição
		(current pointer), inclusive, até o seu final.
-----------------------------------------------------------------------------*/
int truncate2 (FILE2 handle) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Altera o contador de posição (current pointer) do arquivo.
-----------------------------------------------------------------------------*/
int seek2 (FILE2 handle, DWORD offset) {
	initializeEverything();

	if(filesMap[handle] == 0){
		return ERRO;
	}

	t_entradaDir *diretorio = opened_files[handle].


	if((int) offset > (int) diretorio->fileSize || (int) offset < -1) return ERRO;

	if(offset == -1) opened_files[handle].currentPoint = diretorio->fileSize;
	else opened_files[handle].currentPoint = offset;


	return SUCESSO;	

}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um novo diretório.
-----------------------------------------------------------------------------*/
int mkdir2 (char *pathname) {
	
	initializeEverything();
	int i;

	t_entradaDir *diretorio = getLastDirectory(pathname);

	char *nome = fileName(pathname);

	//Checa se o diretório já existe:
	for(i=0; 0<entradasPorDir; i++){
		if(strncmp(nome,diretorio[i].name, strlen(nome)) ==0 ){
			printf("Diretório já existe\n");
			return ERRO;
		}
	}

	//encontra espaço no diretório para armazenar nova entrada
	i=0;
	while(diretorio[i].fileType != 0){
		i++;
	}
	if(i>entradasPorDir){
		printf("diretório cheio\n");
		return ERRO;
	}

	//cria entrada para o diretório:
	t_entradaDir novoDiretorio;
	novoDiretorio.fileType = FILETYPE_DIR;
	memset(novoDiretorio.name,'\0',32);
	strcpy(novoDiretorio.name,nome);
	novoDiretorio.name[31]='\0';

	novoDiretorio.fileSize = (SECTOR_SIZE*superbloco.setoresPorBloco); //tamanho de um bloco
	novoDiretorio.firstBlock = firstBitmapEmpty();
	
	if(novoDiretorio.firstBlock==-1){
		printf("Não houve espaço para o novo diretório\n");
		return ERRO;
	}
	bitmap[novoDiretorio.firstBlock] = 1;
	fat[novoDiretorio.firstBlock] = FEOF;

	diretorio[i] = novoDiretorio;
	
	if(writeFAT() != SUCESSO){
		printf("Fat não pôde ser gravada.\n");
	}
	updatesBitmap();


	t_entradaDir itself;
	char point[] = ".\0";
	itself.fileType=FILETYPE_DIR;
	memset(itself.name,'\0',32);
	strcpy(itself.name,point);

	itself.fileSize = SECTOR_SIZE * superbloco.setoresPorBloco;
	itself.firstBlock = novoDiretorio.firstBlock;

	t_entradaDir *listaDeEntradas = malloc(SECTOR_SIZE * superbloco.setoresPorBloco);

	listaDeEntradas[0]=itself;
	//aqui estariamos gravando o pai

	t_entradaDir vazio;
	vazio.fileType=FILETYPE_INV;
	memset(vazio.name,'\0',32);
	vazio.fileSize=0;
	vazio.firstBlock=0;

	//zera todas as outras entradas do novo diretório
	for(i=1;i<entradasPorDir;i++){
		listaDeEntradas[i]=vazio;
	}

	if(writeBlock((BYTE*) listaDeEntradas, listaDeEntradas[0].firstBlock, superbloco.blocoDirRaiz,superbloco.setoresPorBloco ) != SUCESSO  ){
		return ERRO;
	}

	printf("Diretório criado com sucesso\n");

	return SUCESSO;


}

/*-----------------------------------------------------------------------------
Função:	Função usada para remover (apagar) um diretório do disco.
-----------------------------------------------------------------------------*/
int rmdir2 (char *pathname) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para alterar o CP (current path)
-----------------------------------------------------------------------------*/
int chdir2 (char *pathname) {
	return -1;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para obter o caminho do diretório corrente.
-----------------------------------------------------------------------------*/
int getcwd2 (char *pathname, int size) {
	initializeEverything();

	if(size < MAX_FILE_NAME_SIZE){
		return -1;
	}

	strncpy(pathname,caminhoAtual,size);
	return SUCESSO;
}

/*-----------------------------------------------------------------------------
Função:	Função que abre um diretório existente no disco.
-----------------------------------------------------------------------------*/
DIR2 opendir2 (char *pathname) {
	
	initializeEverything();
	if(opened_files==10){
		printf("Limite de arquivos abertos excedido!\n");
		return ERRO;
	}

	t_entradaDir* aberto;

	if( (aberto = abrirArquivo(pathname)) == NULL){
		printf("Diretório inválido\n");
		return ERRO;
	}

	if(aberto->fileType != FILETYPE_DIR){
		printf("Não é um diretório.\n");
		return ERRO;
	}

	opened_files++;
	
	DIR2 handle;
	
	t_openedFile arquivoAberto;
	arquivoAberto.currentPoint=0;
	arquivoAberto.registro = aberto;

	handle = freeHandle();
	openedFiles[handle] = arquivoAberto;
	filesMap[handle]=1;

	return SUCESSO;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para ler as entradas de um diretório.
-----------------------------------------------------------------------------*/
int readdir2 (DIR2 handle, DIRENT2 *dentry) {
	initializeEverything();

	long int k;
	t_openedFile entradaDiretorio;
	entradaDiretorio=openedFiles[handle];

	t_entradaDir* conteudoDir;

	conteudoDir= (t_entradaDir*)readBlock(entradaDiretorio.registro->firstBlock);

	k = entradaDiretorio.currentPoint;

	while(k<entradasPorDir){

		if(validEntry(conteudoDir[k].fileType) == 0){

			strncpy(dentry->name,conteudoDir[k].name,strlen(conteudoDir[k].name));
			dentry->name[strlen(conteudoDir[k].name)]  = '\0';
			dentry->fileType = conteudoDir[k].fileType;
			dentry->fileSize = conteudoDir[k].fileSize;
			openedFiles[handle].currentPoint = k+1;
			return SUCESSO;
		}
		
		k++;
	}

	return ERRO;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para fechar um diretório.
-----------------------------------------------------------------------------*/
int closedir2 (DIR2 handle) {
	initializeEverything();

	if(filesMap[handle] == 0){
		return ERRO;
	}

	filesMap[handle] =0;
	opened_files--;
	return SUCESSO;
}

/*-----------------------------------------------------------------------------
Função:	Função usada para criar um caminho alternativo (softlink) com
		o nome dado por linkname (relativo ou absoluto) para um 
		arquivo ou diretório fornecido por filename.
-----------------------------------------------------------------------------*/
int ln2 (char *linkname, char *filename) {
	return -1;
}


///////////////////////////FUNÇÕES AUXILIARES////////////////////////////

//Lê os dados do MBR (Sector 0) e retorna um MBR preenchido.
t_MBR readsMBR(){

	unsigned char sectorBuffer[SECTOR_SIZE];
	int teste = read_sector(0,sectorBuffer);

	if( teste != 0 ){
		printf("não leu\n");
		return;
	}
	else {

		t_MBR mbr;

		short* versao = (short*) sectorBuffer;
		mbr.versaoDisco = (int) *versao;
	//	printf("Versão Disco %d \n", mbr.versaoDisco);

		short* tamSetor = (short*)(sectorBuffer+2);
		mbr.tamSetor = (int) *tamSetor;
	//	printf("tamSetor %d \n", mbr.tamSetor);

		short* ByteInicialTabeladeParticoes = (short*)(sectorBuffer+4);
		mbr.BInicialTabela = (int) *ByteInicialTabeladeParticoes;
	//	printf("Byte Inicial Tabela de particoes -: %d \n",mbr.BInicialTabela);

		short* numeroParticoes = (short*)(sectorBuffer+6);
		mbr.NumParticoes = (int) *numeroParticoes;
	//	printf("Numero de Particoes: %d \n", mbr.NumParticoes);

		int* setorInicioPart1 = (int*)(sectorBuffer+8);
		mbr.SetorInicialParticao1 = *setorInicioPart1;
	//	printf("%d\n", mbr.SetorInicialParticao1);

		int* setorFimPart1 = (int*)(sectorBuffer+12);
		mbr.SetorFinalParticao1 = *setorFimPart1;
	//	printf("%d\n", mbr.SetorFinalParticao1);
		return mbr;

		}
}

t_SUPERBLOCO readsSuperblock(){

	unsigned char sectorBuffer[SECTOR_SIZE];
	int teste = read_sector(SUPERBLOCKSECTOR,sectorBuffer);
	

	if( teste != 0 ){
		printf("não leu\n");
		return;
	}
	else {

		t_SUPERBLOCO superblock;

		short* numblocos = (short*) sectorBuffer;
		superblock.numBlocos = *numblocos;

		short* setores = (short*)(sectorBuffer+2);
		superblock.setoresPorBloco = *setores;

		short* tamFAT = (short*)(sectorBuffer+4);
		superblock.tamanhoFAT = *tamFAT;

		short* dirRaiz = (short*)(sectorBuffer+6);
		superblock.blocoDirRaiz = *dirRaiz;

		
		int* bitmap1 = (int*)(sectorBuffer+8);
		int* bitmap2 = (int*)(sectorBuffer+12);
		int* bitmap3 = (int*)(sectorBuffer+16);
		int* bitmap4 = (int*)(sectorBuffer+20);
		int* bitmap5 = (int*)(sectorBuffer+24);
		int* bitmap6 = (int*)(sectorBuffer+28);
		int* bitmap7 = (int*)(sectorBuffer+32);
		int* bitmap8 = (int*)(sectorBuffer+36);

		superblock.bitmap[0]= *bitmap1;
		superblock.bitmap[1]= *bitmap2;
		superblock.bitmap[2]= *bitmap3;
		superblock.bitmap[3]= *bitmap4;
		superblock.bitmap[4]= *bitmap5;
		superblock.bitmap[5]= *bitmap6;
		superblock.bitmap[6]= *bitmap7;
		superblock.bitmap[7]= *bitmap8;

		

		return superblock;
	}
}


//Loads the bitmap with 32 bits of "decimal"
void buildsBitmap(int decimal,int piece){

	int i, result[32];

	for(i=0;i<32;i++)
		result[i]=0;
	
	for(i=0;decimal>0;i++){
		result[i]=decimal%2;
		decimal=decimal/2;
	}

	for(i=32-1;i>=0;i--){
		bitmap[32-1-i + (piece*32)] = result[i];
	}

	return;
}

int initializeFAT(){

	BYTE sector[SECTOR_SIZE];
	int k;

	entriesPerSector = SECTOR_SIZE/4;
	lastindex = (superbloco.tamanhoFAT * entriesPerSector)-1;

	fat = malloc(SECTOR_SIZE * superbloco.tamanhoFAT);

	for(k=0;k < superbloco.tamanhoFAT; k++){

		if(read_sector((FATSECTOR+k), sector) != 0){
			printf("não leu\n");
			return ERRO;
		}
		else{
			memcpy(fat + (entriesPerSector * k), sector, SECTOR_SIZE);
		}
	}

	return SUCESSO;
}

void initializeEverything(){

	int i;
	
	mbr = readsMBR();
	
	superbloco = readsSuperblock();
	

	//Bitmap precisa de no minimo 256 bits = 32 bytes = 8 inteiros. Então ele é representado por 8
	// inteiros no disco, que são lidos para, cada um, preencher 32 bits do bitmap. //CONFERIR
	for(i=0;i<8;i++){
		buildsBitmap(superbloco.bitmap[i],i);
	}

	int teste = initializeFAT();
	
	if(teste != 0){
		printf("Não alocou FAT \n");
		return;
	}

	entradasPorDir = (SECTOR_SIZE* superbloco.setoresPorBloco) / sizeof(t_entradaDir); //Checar Conta
	entradasPorSet = SECTOR_SIZE/sizeof(t_entradaDir);
	tamBloco = SECTOR_SIZE*superbloco.setoresPorBloco;

	for(i=0;i<superbloco.blocoDirRaiz;i++){
		bitmap[i]=1;						//Blocos antes do diretório raiz, estão já ocupados
		fat[i]=1;					    	// pelo superbloco e pela fat.
	}
	
	int teste2 = initializeRoot();
	if(teste2==ERRO){
		printf("root not initiated\n");
		return;
	}

	/* for(i=0; i< superbloco.blocoDirRaiz;i++ ){

		printf("indice bitmap %d\n", bitmap[i]);		//Prints de teste de alocação
		printf("counteúdo fat %d\n", fat[i]);

	}*/
	
	diretorioAtual = raiz;
	strcpy(caminhoAtual,"/\0");

	initialized=1;	//everything setup!
	return;
	

}

int initializeRoot(){

	raiz=malloc(tamBloco);
	int i;
	BYTE sector[SECTOR_SIZE];
	int dataSectorStart = superbloco.setoresPorBloco *superbloco.blocoDirRaiz; //CHECAR SE CONTA ESTÁ CERTA

	for(i=0;i<superbloco.setoresPorBloco;i++){

		int position = dataSectorStart + (superbloco.blocoDirRaiz * superbloco.setoresPorBloco);
		if( read_sector(position+i,sector) != 0 ){

			printf("Não gravou bloco\n");
			return ERRO;
		}

		memcpy(raiz+ (entradasPorSet*i),sector,SECTOR_SIZE);
	}

	bitmap[superbloco.blocoDirRaiz]=1;
	//fat[superbloco.blocoDirRaiz] = 1;
	return SUCESSO;

}

t_entradaDir* getLastDirectory(char* caminho){

	int absoluto = (*caminho == '/');

	t_entradaDir* diretorio;

	char *token = malloc(strlen(caminho) * sizeof(char) );
	int tamToken = sizeof(token);

	memset(token,'\0',tamToken);
	strcpy(token,caminho);

	token = strtok(token,"/");  //Primeira chamada de strtok precisa do string inicial para escanear tokens.
								//Próxima chamada recebe null e usa a posição ao final do ultimo token
								//para começar a escanear por mais tokens.d
	if(absoluto){
		diretorio = raiz;		//Se caminho dado é absoluto, começa da raiz
	}
	else{
		diretorio = diretorioAtual;		//Se é relativo, começa do diretório atual
	}

	int i=0, achou=0;

	while(token!=NULL){

		char tokenAtual[sizeof(token)];
		strncpy(tokenAtual,token,sizeof(token));

		token=strtok(NULL, "/'");

		if(token==NULL){
			return diretorio;
		}
		else{

			while(strncpy(diretorio[i].name, tokenAtual, strlen(tokenAtual) ) !=0 && i<entradasPorDir){
				i++;
			}
			achou=1;

			if(i<entradasPorDir){
				if(diretorio[i].fileType!=2){
					return NULL;
				}
				else{
					diretorio = (t_entradaDir*) readBlock(diretorio[i].firstBlock);
				}

			}
			else{
				return NULL;
			}
		}
	}

	free(token);

	if(achou==0 && absoluto){
		return raiz;
	}
	else{
		return NULL;
	}


}

int writeBlock(BYTE *buffer,int block, int raiz, int setoresPBloco){

	int i;
	int dataSectorStart = setoresPBloco*raiz; //CHECAR SE CONTA ESTÁ CERTA
	BYTE sector[SECTOR_SIZE];
	memset(sector,'\0',SECTOR_SIZE);

	for(i=0;i<setoresPBloco;i++){

		int position = dataSectorStart + (block * setoresPBloco);
		memcpy(sector,(buffer + (SECTOR_SIZE*i)), SECTOR_SIZE);

		if((write_sector(position+i,sector)) !=0){
			return ERRO;
		}
	}
	return SUCESSO;
}

char * readBlock(int numBloco){
	
	char* block = malloc(SECTOR_SIZE* superbloco.setoresPorBloco); //tamanho de um bloco
	unsigned char sector[SECTOR_SIZE];
	int dataSectorStart = superbloco.setoresPorBloco *superbloco.blocoDirRaiz; //CHECAR SE CONTA ESTÁ CERTA
	int i;

	for(i=0;i<superbloco.setoresPorBloco;i++){
		int position = dataSectorStart + (numBloco * superbloco.setoresPorBloco );

		if(read_sector(position+i,sector) != 0){
			printf("Erro lendo bloco %d, setor %d\n",numBloco,(position+i));
			return NULL;
		}
		memcpy( (block + (SECTOR_SIZE*i)),sector, SECTOR_SIZE );
	}

	return block;
}

char *fileName(char *caminho){
	
	char *nome = strdup(caminho); //duplica caminho recebido e guarda em nome
	char *token = strtok(nome,"/");

	while(token!=NULL){
		nome=strdup(token);
		token=strtok(NULL,"/");

		if(token==NULL){
			return nome;
		}
	}

	free(nome);
	return token;

}

int firstBitmapEmpty(){

	int i;

	for(i=0;i<MAXBLOCOS;i++){
		if(bitmap[i]==0){
			return i;
		}
	}
	if(i>=MAXBLOCOS){
		printf("Não há espaço\n");
		return -1;
	}
}

int writeFAT(){
	
	unsigned char sector[SECTOR_SIZE];
	int i;

	for(i=0;i<superbloco.tamanhoFAT;i++){

		int position = FATSECTOR;
		memcpy(sector,(fat + ((SECTOR_SIZE/sizeof(DWORD) )*i) ), SECTOR_SIZE);

		if(write_sector((position+i),sector) != 0){
			printf("FAT Não escrita\n");
			return ERRO;
		}
	}
	return SUCESSO;
}

//Converte os 256 bits do bitmap para 8 inteiros, salva eles no superbloco, e grava o superbloco.
//IGNORAR GAMBIARRA
void updatesBitmap(){

	/////PROBLEMAS de SegFault em alguns algoritmos de conversão
	//Então fiz tudo no trabalho braçal. 

	int i;
	int base=1;
	int inteiro0=0;
	int inteiro1=0;
	int inteiro2=0;
	int inteiro3=0;
	int inteiro4=0;
	int inteiro5=0;
	int inteiro6=0;
	int inteiro7=0;
	int vetor[32];
	int vetor1[32];
	int vetor2[32];
	int vetor3[32];
	int vetor4[32];
	int vetor5[32];
	int vetor6[32];
	int vetor7[32];

	for(i=0;i<32;i++)
	{
		vetor[i] = bitmap[i];
		vetor1[i]=bitmap[i+32];
		vetor2[i]=bitmap[i+(2*32)];
		vetor3[i]=bitmap[i+(3*32)];
		vetor4[i]=bitmap[i+(4*32)];
		vetor5[i]=bitmap[i+(5*32)];
		vetor6[i]=bitmap[i+(6*32)];
		vetor7[i]=bitmap[i+(7*32)];
	}

	/*
	for(i=0;i<32;i++)
	{
		printf("%d",vetor[i]);
	}
	printf("\n");

	for(i=0;i<32;i++)
	{
		printf("%d",vetor2[i]);
	}
	printf("\n"); */
	

	for(i=31;i>=0;i--){
		inteiro0= inteiro0 + (vetor[i] * base);
		
		base=base*2;
	}

	base=1;

	for(i=31;i>=0;i--){
		inteiro1= inteiro1 + (vetor1[i] * base);
		
		base=base*2;
	}

	base=1;

	
	for(i=31;i>=0;i--){
		inteiro2= inteiro2 + (vetor2[i] * base);
		
		base=base*2;
	}

	base=1;

	for(i=31;i>=0;i--){
		inteiro3= inteiro3 + (vetor3[i] * base);
		
		base=base*2;
	}

	base=1;

	for(i=31;i>=0;i--){
		inteiro4= inteiro4 + (vetor4[i] * base);
		
		base=base*2;
	}

	base=1;

	for(i=31;i>=0;i--){
		inteiro5= inteiro5 + (vetor5[i] * base);
		
		base=base*2;
	}

	base=1;

	for(i=31;i>=0;i--){
		inteiro6= inteiro6 + (vetor6[i] * base);
		
		base=base*2;
	}

	base=1;
	for(i=31;i>=0;i--){
		inteiro7= inteiro7 + (vetor7[i] * base);
		
		base=base*2;
	}
	

	superbloco.bitmap[0] = inteiro0;
	superbloco.bitmap[1] = inteiro1;
	superbloco.bitmap[2] = inteiro2;
	superbloco.bitmap[3] = inteiro3;
	superbloco.bitmap[4] = inteiro4;
	superbloco.bitmap[5] = inteiro5;
	superbloco.bitmap[6] = inteiro6;
	superbloco.bitmap[7] = inteiro7;

	
	BYTE sector[SECTOR_SIZE];
	t_SUPERBLOCO superblock = superbloco;
	memcpy(sector,&superblock, SECTOR_SIZE);
	int teste = write_sector(SUPERBLOCKSECTOR, sector);
	
	if(teste!=0){
		printf("não guardou bloco\n");
	}

}

int validEntry(BYTE filetype){

	if(filetype==FILETYPE_DIR || filetype == FILETYPE_ARQ || filetype == FILETYPE_LINK){
		return 0;
	}
	else {
		return -1;
	}
}

t_entradaDir* abrirArquivo(char* caminho){

	t_entradaDir* ArqDir;
	int i;

	if( (ArqDir = getLastDirectory(caminho)) == NULL ){
		printf("diretório inexistente\n");
		return NULL;
	}

	if((strncmp(caminho,"/",strlen(caminho))) == 0){

		return raiz;
	}

	char* nomearquivo = fileName(caminho);

	if(nomearquivo==NULL){
		printf("nome invalido\n");
		return NULL;
	}

	for(i=0;i<entradasPorDir;i++){

		if(strncmp(nomearquivo,ArqDir[i].name,strlen(nomearquivo)) ==0){

			if(ArqDir[i].fileType == FILETYPE_LINK){
				char* link = readBlock(ArqDir[i].firstBlock);
				return abrirArquivo(link);
			}
			else{
				return &ArqDir[i];
			}
		}
	}
	printf("Arquivo não encontrado\n");
	return NULL;

}

int freeHandle(){
	int handle;

	for(handle=0;handle < MAX_OPENEDF;handle++){

		if(filesMap[handle]==0){
			return handle;
		}
	}
	return ERRO;
}
