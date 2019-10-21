#include<stdio.h>
#include<string.h>
#include<stdlib.h>
#include<json/json.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xmlschemastypes.h>
#include <fcntl.h>

// Replaces given character set with another in a string. Used for fixing double quotes in csv.
// Source : https://www.quora.com/How-do-I-replace-a-character-in-a-string-with-3-characters-using-C-programming-language
char* replace(char* str, char* a, char* b) {
    int len  = strlen(str);
    int lena = strlen(a), lenb = strlen(b);
    char *p;
    for (p = str; p = strstr(p, a); ++p) {
        if (lena != lenb)
            memmove(p+lenb, p+lena,len - (p - str) + lenb);
        memcpy(p, b, lenb);
    }
    return str;
}
// Takes json root object first and creates xml nodes and attach them to parent.
void json_parse_xml_build(json_object * jobj,xmlNodePtr parent) {
    xmlNodePtr temp;
    int i;
    enum json_type type;
    json_object_object_foreach(jobj, key, val) {
    type = json_object_get_type(val);
    switch (type) {
        case json_type_string:
          xmlNewChild(parent,NULL, BAD_CAST key,BAD_CAST json_object_get_string(val));
          break;
        case json_type_int:
          xmlNewChild(parent,NULL, BAD_CAST key,BAD_CAST json_object_get_string(val));
          break;
        case json_type_object:
          temp = xmlNewChild(parent,NULL, BAD_CAST key,NULL);
          json_parse_xml_build(val,temp);
          break;
        case json_type_array:
          for(i = 0;i < json_object_array_length(val);i++){
            xmlNewChild(parent,NULL,BAD_CAST key,json_object_get_string(json_object_array_get_idx(val,i)));
          }
          break;
        case json_type_double:
          xmlNewChild(parent,NULL, BAD_CAST key,BAD_CAST json_object_get_string(val));
          break;
        case json_type_boolean:
          xmlNewChild(parent,NULL, BAD_CAST key,BAD_CAST json_object_get_string(val));
          break;
        case json_type_null:
          xmlNewChild(parent,NULL, BAD_CAST key,NULL);
          break;
        default:
          printf("Doesn't fit to anything");
        }
    }
}
// Takes xml file and converts it to json.
// Not working for arrays.
void xml_parse_json_build(xmlNodePtr xmlNd,json_object * parent){
  json_object * temp = json_object_new_object();
  xmlAttrPtr attribute = NULL;
  if(xmlNd->children != NULL){
    xml_parse_json_build(xmlNd->children,temp);
  }
  if(xmlNd->type == XML_ELEMENT_NODE){
    attribute = xmlNd->properties;
    if(xmlNd->children->type == XML_TEXT_NODE && xmlNd->children->next == NULL){
      if(attribute){
        temp = json_object_new_object();
        json_object_object_add(temp,xmlNd->name,json_object_new_string(xmlNd->children->content));
      }
      else{
        temp = json_object_new_string(xmlNd->children->content);
      }
    }
    while(attribute) {
      xmlChar* value = xmlNodeListGetString(xmlNd->doc,attribute->children, 1);
      json_object_object_add(temp,attribute->name,json_object_new_string(attribute->children->content));
      xmlFree(value);
      attribute = attribute->next;
    }

    json_object_object_add(parent,xmlNd->name,temp);
  }
  if(xmlNd->next != NULL){
    xml_parse_json_build(xmlNd->next,parent);
  }
}
void xml_parse_csv_build(xmlNodePtr xmlNd,FILE * fp){
    xmlNodePtr temp;
    if(xmlNd->children != NULL){
        if(xmlNd->children->next != NULL){
            temp = xmlNd->children->next;
        }
        else{
            printf("xmlNd->children->next is null. xmlNd->children=%s\n",xmlNd->children->name);
        }
    }
    else{
        printf("xmlNd->children is null. xmlNd=%s\n",xmlNd->name);
    }
    fprintf(fp,"%s",temp->children->content);
    temp = temp->next;
    while(temp->next != NULL){
      if(temp->type == XML_ELEMENT_NODE){
        if(temp->children == NULL)
          fprintf(fp,",");
        else
          fprintf(fp,",%s",temp->children->content);
      }
      if(temp->next != NULL)
        temp = temp->next;
    }
    fprintf(fp,"\n");
    if(xmlNd->next != NULL){
        temp = xmlNd->next;
        while(temp != NULL && strcmp(temp->name,"text") == 0){
            temp = temp->next;
        }
        if(temp != NULL)
            xml_parse_csv_build(temp,fp);
    }
}
// Necessary declarations for file operations. Used for removing last char from a file.
// Source : https://stackoverflow.com/a/1271126
int ftruncate(int fd, off_t length);
int fileno(FILE *stream);
// Parses json file and writes csv file.
void json_parse_csv_build(FILE * fp,json_object * jobj){
    enum json_type type;
    int i;
    json_object_object_foreach(jobj,key,val){
        type = json_object_get_type(val);
        switch (type) {
          case json_type_object:
            json_parse_csv_build(fp,val);
            break;
          case json_type_array:
            for(i = 0;i < json_object_array_length(val);i++){
              // Writes column names.
              if(i == 0){
                json_object_object_foreach(json_object_array_get_idx(val,i),arrKey,arrVal){
                  fprintf(fp,"%s,",arrKey);
                }
                // Removes last comma.
                fseek(fp, -1, SEEK_END);
                ftruncate(fileno(fp), ftell(fp));
                fprintf(fp,"\n");
              }
              // Writes row.
              json_object_object_foreach(json_object_array_get_idx(val,i),arrKey,arrVal){
                if(json_object_get_type(arrVal) == json_type_null){
                  fprintf(fp,",");
                }
                else{
                  fprintf(fp,"%s,",json_object_get_string(arrVal));
                }
              }
              // Removes last comma.
              fseek(fp, -1, SEEK_END);
              ftruncate(fileno(fp), ftell(fp));
              fprintf(fp,"\n");
            }
            break;
        }
    }
}
// Reading file by name, used for converting json to string.
char* file_read (const char* filename) {
  FILE* fp;
  char* buffer;
  long  fsize;

  // Open the file 
  fp = fopen (filename, "r");

  if (fp == NULL)
    {
      return NULL;
    }

  // Get the size of the file
  fseek (fp, 0, SEEK_END);
  fsize = ftell (fp) + 1;
  fseek (fp, 0, SEEK_SET);

  // Allocate the buffer
  buffer = calloc (fsize, sizeof (char));

  if (buffer == NULL)
    {
      fclose (fp);
      return NULL;
    }

  // Read the file
  fread (buffer, sizeof (char), fsize, fp);

  // Close the file
  fclose (fp);

  // Return the buffer
  return buffer;
}

// Main declarations for functions.
void xmlValidate(char *XMLName,char *XSDName);
void xmlToJSON(char *fileName,char *OutputName);
void xmlToCSV(char *fileName,char *OutputName);
void jsonToXML(char *fileName,char *OutputName);
void jsonToCSV(char *fileName,char *OutputName);
void csvToXML(char *fileName,char *OutputName);
void csvToJSON(char *fileName,char *OutputName);

int main(int argc, char *argv[]){
  if(strcmp(argv[3],"1") == 0){
    csvToXML(argv[1],argv[2]);
  }
  else if(strcmp(argv[3],"2") == 0){
    xmlToCSV(argv[1],argv[2]);
  }
  else if(strcmp(argv[3],"3") == 0){
    xmlToJSON(argv[1],argv[2]);
  }
  else if(strcmp(argv[3],"4") == 0){
    jsonToXML(argv[1],argv[2]);
  }
  else if(strcmp(argv[3],"5") == 0){
    csvToJSON(argv[1],argv[2]);
  }
  else if(strcmp(argv[3],"6") == 0){
    jsonToCSV(argv[1],argv[2]);
  }
  else if(strcmp(argv[3],"7") == 0){
    xmlValidate(argv[1],argv[2]);
  }
  else{
    printf("Invalid type");
  }
  return 0;
}
// Reads csv line by line and creates XML.
void csvToXML(char *fileName,char *outputName){
  FILE *fp;
  xmlDocPtr xmldc = NULL;
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "root");
  xmlNodePtr temp;
  char *buffer = malloc(512);
  xmldc = xmlNewDoc(BAD_CAST "1.0");
  xmlDocSetRootElement(xmldc,root_node);
  char c;

  fp = fopen (fileName, "r" );
  if( !fp ) perror(fileName),exit(1);

  char *rdLine;
  rdLine = malloc(1000);
  int i = 0;
  // Reading first line which is column names.
  while((c = fgetc(fp)) != '\n') {
          *(rdLine+i) = c;
          i++;
  }
  *(rdLine+i) = '\0';
  char delim[] = ",";
  char **header = malloc(2000);
  char **splitData = malloc(2000);
  char *ptr = calloc(2000,1);
  ptr = strtok(rdLine, delim);
  char *ptr2 = calloc(2000,1);
  i = 0;
  // Splitting column names and storing them.
  while(ptr != NULL)
  {
    header[i] = calloc(strlen(ptr)+1,1);
    strcpy(header[i],ptr);
    replace(header[i]," ","_");
    i++;
    ptr = strtok(NULL, delim);
  }
  int test = i;
  while(c != EOF){
    i = 0;
    // Resetting rdLine
    free(rdLine);
    rdLine = calloc(1000,1);
    // Reading line/row.
    while((c = fgetc(fp)) != '\n' && c != EOF) {
            *(rdLine+i) = c;
            i++;
    }
    *(rdLine+i) = '\0';
    // Two times replace because of possible double consecutive empty elements.
    replace(rdLine,",,",",NULL,");
    replace(rdLine,",,",",NULL,");
    i = 0;
    // Splitting rows.
    ptr2 = strtok(rdLine,delim);
    while(ptr2 != NULL){
      splitData[i] = calloc(strlen(ptr2)+1,1);
      strcpy(splitData[i],ptr2);
      i++;
      ptr2 = strtok(NULL,delim);
    }
    i = 0;
    // Creating row.
    temp = xmlNewChild(root_node,NULL,BAD_CAST "row",NULL);
    while(i < test){
      if(strcmp(splitData[i],"NULL") == 0){
        xmlNewChild(temp,NULL,BAD_CAST header[i],NULL);
      }
      else{
        xmlNewChild(temp,NULL,BAD_CAST header[i],BAD_CAST splitData[i]);
      }
      i++;
    }
  }
  xmlSaveFormatFileEnc(outputName, xmldc, "UTF-8", 1);
	xmlSaveFormatFileEnc("-", xmldc, "UTF-8", 1);
  xmlFreeDoc(xmldc);
  xmlCleanupParser();
  xmlMemoryDump();
  fclose(fp);
  free(buffer);
  free(header);
  free(ptr);
  free(ptr2);
  free(splitData);
}
// Reads csv line by line and creates JSON. Same reading algorithm.
void csvToJSON(char *fileName,char *outputName){
  FILE *fp;
  json_object * root_node = json_object_new_object();
  json_object * rows = json_object_new_array();
  json_object * temp;
  char *buffer = malloc(512);
  char c;

  fp = fopen (fileName, "r" );
  if( !fp ) perror(fileName),exit(1);

  char *rdLine;
  rdLine = malloc(2000);
  int i = 0;
  while((c = fgetc(fp)) != '\n') {
          *(rdLine+i) = c;
          i++;
  }
  *(rdLine+i) = '\0';
  char delim[] = ",";
  char **header = malloc(2000);
  char **splitData = malloc(2000);
  char *ptr = calloc(2000,1);
  ptr = strtok(rdLine, delim);
  char *ptr2 = calloc(2000,1);
  i = 0;
  while(ptr != NULL)
  {
    header[i] = calloc(strlen(ptr)+1,1);
    strcpy(header[i],ptr);
    replace(header[i]," ","_");
    i++;
    ptr = strtok(NULL, delim);
  }
  int test = i;
  while(c != EOF){
    free(rdLine);
    rdLine = calloc(1000,1);
    i = 0;
    while((c = fgetc(fp)) != '\n' && c != EOF) {
            *(rdLine+i) = c;
            i++;
    }
    *(rdLine+i) = '\0';
    replace(rdLine,",,",",NULL,");
    i = 0;
    ptr2 = strtok(rdLine,delim);
    while(ptr2 != NULL){
      splitData[i] = calloc(strlen(ptr2)+1,1);
      strcpy(splitData[i],ptr2);
      i++;
      ptr2 = strtok(NULL,delim);
    }
    i = 0;
    temp = json_object_new_object();
    while(i < test){
      if(strcmp(splitData[i],"NULL") == 0){
        json_object_object_add(temp,header[i],NULL);
      }
      else
        json_object_object_add(temp,header[i],json_object_new_string(splitData[i]));
      i++;
    }
    json_object_array_add(rows,temp);
    temp = NULL;
  }
  json_object_object_add(root_node,"rows",rows);
  json_object_to_file(outputName,root_node);
  fclose(fp);
  free(buffer);
  free(header);
  free(ptr);
  free(ptr2);
  free(splitData);
}
// Read json as json_object. Convert it to XML.
void jsonToXML(char *fileName,char *outputName){
  /// Initializing xml file by creating root.
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "root");
  xmlDocPtr xmldc = NULL;
  xmldc = xmlNewDoc(BAD_CAST "1.0");
  xmlDocSetRootElement(xmldc,root_node);
  // Reading json as string.
  char *rdLine = file_read(fileName);

  json_object * jobj = json_tokener_parse(rdLine);
  json_parse_xml_build(jobj,root_node);
  xmlSaveFormatFileEnc(outputName, xmldc, "UTF-8", 1);
  xmlSaveFormatFileEnc("-", xmldc, "UTF-8", 1);
  xmlFreeDoc(xmldc);
  xmlCleanupParser();
  xmlMemoryDump();
}
// Read xml as tree. Converts it to JSON.
void xmlToJSON(char *fileName,char *outputName){
  xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
  json_object * root_node = json_object_new_object();
  json_object * temp_json;
  json_object * temp_json_array;

	doc = xmlReadFile(fileName, NULL, 0);

	if (doc == NULL) {
		printf("error: could not parse file \n");
	}

  root_element = xmlDocGetRootElement(doc);
  xml_parse_json_build(root_element,root_node);
  json_object_to_file(outputName,root_node);
  xmlFreeDoc(doc);
  xmlCleanupParser();
}
void xmlToCSV(char *fileName,char *outputName){
  xmlDoc *doc = NULL;
	xmlNode *root_element = NULL;
	doc = xmlReadFile(fileName, NULL, 0);
	if (doc == NULL) {
		printf("error: could not parse file \n");
	}
    root_element = xmlDocGetRootElement(doc);
    FILE * fp = fopen(outputName,"w");
    if(!fp){
        printf("Error opening file with fopen");
        return;
    }
    xmlNodePtr starting = root_element->children->next;
    xmlNodePtr temp = starting->children;
    if(temp->type != XML_ELEMENT_NODE || strcmp(temp->name,"text") == 0) {
        temp = temp->next;
    }
    fprintf(fp,"%s",temp->name);
    temp = temp->next;
    while(temp->next != NULL){
        if(temp->type == XML_ELEMENT_NODE && strcmp(temp->name,"text") < 0) {
            fprintf(fp,",%s",temp->name);
        }
        temp = temp->next;
    }
    fprintf(fp,"\n");
    xml_parse_csv_build(starting,fp);
    xmlFreeDoc(doc);
    xmlCleanupParser();
}
void jsonToCSV(char *fileName,char *outputName){
  char *rdLine = file_read(fileName);
  json_object * jobj = json_tokener_parse(rdLine);
  FILE * fp = fopen(outputName,"w");
  if(!fp){
      printf("Error opening file with fopen");
      return;
  }
  json_parse_csv_build(fp,jobj);
  fclose(fp);
}
void xmlValidate(char *XMLName,char *XSDName){
  xmlDocPtr doc;
  xmlSchemaPtr schema = NULL;
  xmlSchemaParserCtxtPtr ctxt;
  char *XMLFileName = XMLName;
  char *XSDFileName = XSDName;
    
  xmlLineNumbersDefault(1); //set line numbers, 0> no substitution, 1>substitution
  ctxt = xmlSchemaNewParserCtxt(XSDFileName); //create an xml schemas parse context
  schema = xmlSchemaParse(ctxt); //parse a schema definition resource and build an internal XML schema
  xmlSchemaFreeParserCtxt(ctxt); //free the resources associated to the schema parser context
    
  doc = xmlReadFile(XMLFileName, NULL, 0); //parse an XML file
  if (doc == NULL)
  {
    fprintf(stderr, "Could not parse %s\n", XMLFileName);
  }
  else
  {
    xmlSchemaValidCtxtPtr ctxt;  //structure xmlSchemaValidCtxt, not public by API
    int ret;
        
    ctxt = xmlSchemaNewValidCtxt(schema); //create an xml schemas validation context 
    ret = xmlSchemaValidateDoc(ctxt, doc); //validate a document tree in memory
    if (ret == 0) //validated
    {
      printf("%s validates\n", XMLFileName);
    }
    else if (ret > 0) //positive error code number
    {
      printf("%s fails to validate\n", XMLFileName);
    }
    else //internal or API error
    {
      printf("%s validation generated an internal error\n", XMLFileName);
    }
    xmlSchemaFreeValidCtxt(ctxt); //free the resources associated to the schema validation context
    xmlFreeDoc(doc);
  }
    // free the resource
  if(schema != NULL)
    xmlSchemaFree(schema); //deallocate a schema structure

  xmlSchemaCleanupTypes(); //cleanup the default xml schemas types library
  xmlCleanupParser(); //cleans memory allocated by the library itself 
  xmlMemoryDump(); //memory dump
}
