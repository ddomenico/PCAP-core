/*       LICENCE
* PCAP - NGS reference implementations and helper code for mapping (originally part of ICGC/TCGA PanCancer)
* Copyright (C) 2014-2018 ICGC PanCancer Project
* Copyright (C) 2018-2021 Cancer, Ageing and Somatic Mutation, Genome Research Limited
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not see:
*   http://www.gnu.org/licenses/gpl-2.0.html
*/

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>
#include <string.h>

#define BUF_SIZE 8192

char *dict = NULL;
char **dict_name;
char **dict_entry;
int dict_entries = 0;

void print_version (int exit_code){
  printf ("%s\n",VERSION);
	exit(exit_code);
}

int check_exist(char *fname){
	FILE *fp;
	if((fp = fopen(fname,"r"))){
		fclose(fp);
		return 1;
	}
	return 0;
}

void print_usage (int exit_code){
	printf("Usage: reheadSQ -m fa.dict -a assembly -s species\n\n");
	printf("Takes sam format from stdin, prints header where SQ lines are replaced with dict contents and rest of stdin to stdout.\n\n");
	printf("-d  --dict [file]    Path to fasta dict file (as generated by 'samtools dict -a ASSEMBLY -s SPECIES genome.fasta')\n");
  printf("-h  --help           Display this usage information.\n");
	printf("-v  --version        Prints the version number.\n\n");

  exit(exit_code);
}

void setup_options(int argc, char *argv[]){
	const struct option long_opts[] =
	{
             	{"version",no_argument, 0, 'v'},
							{"dict", required_argument, 0, 'd'},
             	{"help", no_argument, 0, 'h'},
             	{ NULL, 0, NULL, 0}
   }; //End of declaring opts

   int index = 0;
   int iarg = 0;

   //Iterate through options
   while((iarg = getopt_long(argc, argv, "d:h",long_opts, &index)) != -1){
    switch(iarg){
			case 'v':
      	print_version(0);
      	break;
      case 'd':
        dict = optarg;
        break;
			case 'h':
				print_usage (0);
				break;
			case '?':
        print_usage (1);
        break;
      default:
      	print_usage (0);
   	}; // End of args switch statement

   }//End of iteration through options

     //Do some checking to ensure required arguments were passed and are accessible files
  if(check_exist(dict) != 1){
    fprintf(stderr,"Dict file %s does not appear to exist.\n",dict);
    print_usage(1);
  }
}

int line_count(char *file){
  FILE *in = fopen(file,"r");
  if(in==NULL){
    fprintf(stderr,"Error opening input file %s for line count:%d.\n",file,errno);
    return -1;
  }
  char line [ 5000 ];
  int line_count=0;
	while ( fgets(line,sizeof(line),in) != NULL ){
	  if(strncmp(line, "@SQ" ,3)==0){
	    line_count++;
	  }
	}
	//Close input file
  fclose(in);
  return line_count;
}

char *get_contig_name_fromSQ_line(const char *line){
  char *tmp_line = malloc(sizeof(char) * (strlen(line)+1));
  strcpy(tmp_line,line);
  char *nom = NULL;
  char *tag = strtok(tmp_line,"\t");
  char *tmp = NULL;
  while(tag != NULL){
    int chk=0;
    tmp = malloc(sizeof(char) * 5000);
    chk = sscanf(tag,"SN:%[^\t\n]",tmp);
    if(chk>0){
      nom = malloc(sizeof(char) * (strlen(tmp)+1));
      strcpy(nom,tmp);
      tag = strtok(NULL,"\t");
      continue;
    }
    tag = strtok(NULL,"\t");
  }
  if(tmp) free(tmp);
  free(tmp_line);
  if(nom==NULL){
    fprintf(stderr,"Error fetching contig name given line %s\n",line);
    exit(1);
  }
  return nom;
}

char *get_dict_sq_line_by_name(const char *name){
  int i=0;
  for(i=0;i<dict_entries;i++){
    if(strcmp(dict_name[i],name)==0){
      return dict_entry[i];
    }
  }
  fprintf(stderr,"No @SQ line found for contig named %s\n",name);
  exit(1);
}

void read_dict_file(char *dict_path){

  //Get number of entries
  dict_entries = line_count(dict_path);

  //Initialise the storage of the dict entries
  dict_name = malloc(dict_entries * sizeof(char *));
  dict_entry = malloc(dict_entries * sizeof(char *));

  FILE *df = fopen(dict_path,"r");
  if(df == NULL){
    fprintf(stderr,"Error opening dict file %s: %d\n",dict_path,errno);
    exit(1);
  }

  //Read contents line by line
  char line [ 5000 ];
  int i = 0;
	while ( fgets(line,sizeof(line),df) != NULL ){
    //Only want the @SQ lines
    if(strncmp(line, "@SQ" ,3)==0){
      //Get sequence name
      char *nom = get_contig_name_fromSQ_line(line);
      dict_name[i] = malloc(sizeof(char) * (strlen(nom)+1));
      strcpy(dict_name[i],nom);
      free(nom);
      dict_entry[i] = malloc(sizeof(char) * (strlen(line)+1));
      strcpy(dict_entry[i],line);
      i++;
    }//End of checking for sequence header
	}//End of while loop reading file line by line

  //Close input file
  fclose(df);
}

int main (int argc, char* argv[]){

  setup_options(argc, argv);

  read_dict_file(dict);

  //Read from stdin and write to stdout
  char *line = NULL;
  ssize_t read;
  size_t linelen = 0;

  while((read = getline(&line,&linelen,stdin)) != -1){
    if(strncmp(line, "@", 1) == 0){//If we're matching a header
      if(strncmp(line, "@SQ" ,3)==0){//Code to replace/append to SQ lines here @SQ
        char *nom = get_contig_name_fromSQ_line(line);
        char *new = get_dict_sq_line_by_name(nom);
        fprintf(stdout,"%s",new);
        fflush(stdout);
        free(nom);
      }else{//Not a SQ header
        fprintf(stdout,"%s",line);
      }
    }else{ // We're no longer matching a header
      fprintf(stdout,"%s",line);
      break;
    }//End of checking for a header
  }
  //Another loop, this time we know we're past the SQ headers so it goes straight to stdout.
  while((read = getline(&line,&linelen,stdin)) != -1){
    fprintf(stdout,"%s",line);
  }
  fflush(stdout);
  free(line);

  int j=0;
  for(j=0;j<dict_entries;j++){
    free(dict_name[j]);
    free(dict_entry[j]);
  }
  free(dict_name);
  free(dict_entry);
  return 0;
}
