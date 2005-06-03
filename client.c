
/*
 * cmpic.h
 *
 * (C) Copyright IBM Corp. 2005
 *
 * THIS FILE IS PROVIDED UNDER THE TERMS OF THE COMMON PUBLIC LICENSE
 * ("AGREEMENT"). ANY USE, REPRODUCTION OR DISTRIBUTION OF THIS FILE
 * CONSTITUTES RECIPIENTS ACCEPTANCE OF THE AGREEMENT.
 *
 * You can obtain a current copy of the Common Public License from
 * http://oss.software.ibm.com/developerworks/opensource/license-cpl.html
 *
 * Author:        Adrian Schuur <schuur@de.ibm.com>
 *
 * Description:
 *
 * CMPI Client function tables.
 *
*/

#include "cmci.h"
#include "utilStringBuffer.h"
#include "utilList.h"
#include "native.h"

#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "cimXmlParser.h"

typedef struct _CMCIConnectionFT {
    char *(*genRequest)(ClientEnc *cle, char *op, CMPIObjectPath *cop, int cls, int keys);
    char *(*addPayload)(CMCIConnection *, UtilStringBuffer *pl);
    char *(*getResponse)(CMCIConnection *con, CMPIObjectPath *cop);
    void (*initializeHeaders)(CMCIConnection *con);
    void (*reset)(CMCIConnection *);
} CMCIConnectionFT;

struct _ClientEnc {
   CMCIClient enc;
   CMCIClientData data;
   CMCIConnection *connection;
};   

struct _CMCIConnection {
    CMCIConnectionFT *ft;        // The handle to the curl object
    CURL *mHandle;               // The list of headers sent with each request
    struct curl_slist *mHeaders; // The body of the request
    UtilStringBuffer *mBody;     // The uri of the request
    UtilStringBuffer *mUri;      // The username/password used in authentication
    UtilStringBuffer *mUserPass; // Used to store the HTTP response
    UtilStringBuffer *mResponse;
};

static const char *headers[] = {
    "Content-type: application/xml; charset=\"utf-8\"",
    "Connection: Keep-Alive, TE",
    "CIMProtocolVersion: 1.0",
    "CIMOperation: MethodCall"
};
#define NUM_HEADERS ((sizeof(headers))/(sizeof(headers[0])))

static const char xmlHeader[]={
   "<?xml version=\"1.0\" encoding=\"utf-8\" ?>\n"
   "<CIM CIMVERSION=\"2.0\" DTDVERSION=\"2.0\">\n"
   "<MESSAGE ID=\"4711\" PROTOCOLVERSION=\"1.0\">"
   "<SIMPLEREQ>\n"
};

static char *getErrorMessage(CURLcode err)
{
    static char error[128];
#if LIBCURL_VERSION_NUM >= 0x071200
    return curl_easy_strerror(rv);
#else
    sprintf(error,"CURL error: %d",err);
    return error;
#endif
}

void list2StringBuffer(UtilStringBuffer *sb, UtilList *ul, char *sep)
{
   void *e;
   for (e=ul->ft->getFirst(ul); e; e=ul->ft->getNext(ul)) {
      sb->ft->appendChars(sb,(char*)e);
      sb->ft->appendChars(sb,sep);
   }
}

static size_t writeCb(void *ptr, size_t size, size_t nmemb, void *stream)
{
    UtilStringBuffer *sb=(UtilStringBuffer*)stream;
    unsigned int length = size * nmemb;
    sb->ft->appendBlock(sb, ptr, length);
    return length;
}

static char *addPayload(CMCIConnection *con, UtilStringBuffer *pl)
{
//    con->mBody = pl;
//    if (dumpXml)
//        cerr << "To server: " << pl << endl;

    CURLcode rv;

    rv = curl_easy_setopt(con->mHandle, CURLOPT_POSTFIELDS, pl->ft->getCharPtr(pl));
    if (rv) return getErrorMessage(rv);
    rv = curl_easy_setopt(con->mHandle, CURLOPT_POSTFIELDSIZE, pl->ft->getSize(pl));
    if (rv) return getErrorMessage(rv);
    else return NULL;
}

static char* genRequest(ClientEnc *cle, char *op, CMPIObjectPath *cop, int cls, int keys) 
{
   CMCIConnection *con=cle->connection;
   CMCIClientData *cld=&cle->data;
   CURLcode rv;
   UtilStringBuffer *sb=newStringBuffer(1024);
   char userPwd[256];
   char method[256]="CIMMethod: ";
    
  
   if (!con->mHandle) return "Unable to initialize curl interface.";
    
//    if (!supportsSSL() && url.scheme == "https")
//        throw HttpException("this curl library does not support https urls.");

   con->mResponse->ft->reset(con->mResponse);
   
   con->mUri->ft->reset(con->mUri);
   con->mUri->ft->append6Chars(con->mUri,cld->scheme,"://",cld->hostName,":",cld->port,"/cimom");

   sb->ft->appendChars(sb,"CIMObject: ");
    //url.ns.toStringBuffer(sb,"%2F");
    
    /* Initialize curl with the url */
   rv = curl_easy_setopt(con->mHandle, CURLOPT_URL, con->mUri->ft->getCharPtr(con->mUri));

    /* Disable progress output */
   rv = curl_easy_setopt(con->mHandle, CURLOPT_NOPROGRESS, 1);

    /* This will be a HTTP post */
   rv = curl_easy_setopt(con->mHandle, CURLOPT_POST, 1);

    /* Disable SSL verification */
   rv = curl_easy_setopt(con->mHandle, CURLOPT_SSL_VERIFYHOST, 0);
   rv = curl_easy_setopt(con->mHandle, CURLOPT_SSL_VERIFYPEER, 0);

    /* Set username and password */
   if (cld->user) {
      strcpy(userPwd,cld->user);
      if (cld->pwd) {
         strcat(userPwd,":");
         rv = curl_easy_setopt(con->mHandle, CURLOPT_USERPWD, userPwd);
      }   
   }
    
    // Initialize default headers
    con->ft->initializeHeaders(con);

    // Add CIMMethod header
    strcat(method,op);
    con->mHeaders = curl_slist_append(con->mHeaders, method);

    // Add CIMObject header
    con->mHeaders = curl_slist_append(con->mHeaders, sb->ft->getCharPtr(sb));


    // Set all of the headers for the request
    rv = curl_easy_setopt(con->mHandle, CURLOPT_HTTPHEADER, con->mHeaders);

    // Set up the callbacks to store the response
    rv = curl_easy_setopt(con->mHandle, CURLOPT_WRITEFUNCTION, writeCb);

    // Use CURLOPT_FILE instead of CURLOPT_WRITEDATA - more portable
    rv = curl_easy_setopt(con->mHandle, CURLOPT_FILE, con->mResponse);
    
    // Fail if we receive an error (HTTP response code >= 300)
    rv = curl_easy_setopt(con->mHandle, CURLOPT_FAILONERROR, 1);

    // Turn this on to enable debugging
    // rv = curl_easy_setopt(mHandle, CURLOPT_VERBOSE, 1);
    
    return NULL;
}

char *getResponse(CMCIConnection *con, CMPIObjectPath *cop)
{
    CURLcode rv;

    rv = curl_easy_perform(con->mHandle);
    if (rv) {
        long responseCode = -1;
        char *error;
        // Use CURLINFO_HTTP_CODE instead of CURLINFO_RESPONSE_CODE
        // (more portable to older versions of curl)
        curl_easy_getinfo(con->mHandle, CURLINFO_HTTP_CODE, &responseCode);
        if (responseCode == 401) {
            error = (con->mUserPass->ft->getSize(con->mUserPass) > 0) ? "Invalid username/password." :
                                               "Username/password required.";
        }
        else error = getErrorMessage(rv);
        return error;
    }

    if (con->mResponse->ft->getSize(con->mResponse) == 0)
        return "No data received from server.";
    
    return NULL;
}

static void initializeHeaders(CMCIConnection *con)
{
     unsigned int i;
    
    if (con->mHeaders) {
        curl_slist_free_all(con->mHeaders);
        con->mHeaders = NULL;
    }
    for (i = 0; i < NUM_HEADERS; i++)
        con->mHeaders = curl_slist_append(con->mHeaders, headers[i]);
}

CMCIConnectionFT conFt={
  genRequest,
  addPayload,
  getResponse,
  initializeHeaders
};

CMCIConnection *initConnection(CMCIClientData *cld)
{
   CMCIConnection *c=(CMCIConnection*)calloc(1,sizeof(CMCIConnection));  
   c->ft=&conFt;
   c->mHandle = curl_easy_init();
   c->mHeaders = NULL;
   c->mBody = newStringBuffer(256);
   c->mUri = newStringBuffer(256);
   c->mResponse = newStringBuffer(2048);
   
   return c;    
}


extern UtilList *getNameSpaceComponents(CMPIObjectPath * cop);
extern char *keytype2Chars(CMPIType type);
extern void pathToXml(UtilStringBuffer *sb, CMPIObjectPath *cop);

static void emitlocal(UtilStringBuffer *sb, int f) 
{
   if (f) 
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"LocalOnly\"><VALUE>TRUE</VALUE></IPARAMVALUE>\n");
   else 
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"LocalOnly\"><VALUE>FALSE</VALUE></IPARAMVALUE>\n");
}      

static void emitqual(UtilStringBuffer *sb, int f) 
{
   if (f)
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"IncludeQualifiers\"><VALUE>TRUE</VALUE></IPARAMVALUE>\n");
   else
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"IncludeQualifiers\"><VALUE>FALSE</VALUE></IPARAMVALUE>\n");
}

static void emitorigin(UtilStringBuffer *sb, int f) 
{
   if (f)
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"IncludeClassOrigin\"><VALUE>TRUE</VALUE></IPARAMVALUE>\n");
   else
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"IncludeClassOrigin\"><VALUE>FALSE</VALUE></IPARAMVALUE>\n");
}      

static void emitdeep(UtilStringBuffer *sb, int f) 
{
   if (f)
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"DeepInheritance\"><VALUE>TRUE</VALUE></IPARAMVALUE>\n");
   else
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"DeepInheritance\"><VALUE>FALSE</VALUE></IPARAMVALUE>\n");
}



static CMPIEnumeration *enumInstanceNames(CMCIClient* mb,
    CMPIObjectPath* cop, CMPIStatus* rc)
{   
   ClientEnc *cl=(ClientEnc*)mb;
   CMCIConnection *con=cl->connection;
   UtilStringBuffer *sb=newStringBuffer(2048);
   UtilList *nsc;
   CMPIString *cn;
   char *ns,*error;

   cn=cop->ft->getClassName(cop,NULL);
   con->ft->genRequest(cl,"EnumerateInstanceNames",cop,0,0);
   sb->ft->appendChars(sb,xmlHeader);

   nsc=getNameSpaceComponents(cop);
   sb->ft->appendChars(sb,"<IMETHODCALL NAME=\"EnumerateInstanceNames\">"
          "<LOCALNAMESPACEPATH>");
   for (ns=nsc->ft->getFirst(nsc); ns; ns=nsc->ft->getNext(nsc))
      sb->ft->append3Chars(sb,"<NAMESPACE NAME=\"",ns,"\"></NAMESPACE>");
   sb->ft->appendChars(sb,"</LOCALNAMESPACEPATH>\n");
   
   sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"ClassName\"><CLASSNAME NAME=\"",(char*)cn->hdl, 
           "\"/></IPARAMVALUE>\n");

   sb->ft->appendChars(sb,"</IMETHODCALL></SIMPLEREQ>\n</MESSAGE></CIM>");

//   fprintf(stderr,"%s\n",sb->ft->getCharPtr(sb));
   con->ft->addPayload(con,sb);
   
   if ((error=con->ft->getResponse(con,cop))) {
      CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,error);
      return NULL;
   }
//   fprintf(stderr,"%s\n",con->mResponse->ft->getCharPtr(con->mResponse));
   
   ResponseHdr rh=scanCimXmlResponse(con->mResponse->ft->getCharPtr(con->mResponse),cop);

   if (rh.errCode!=0) {
      CMSetStatusWithChars(rc,rh.errCode,rh.description);
      return NULL;
   }
   
   if (rh.rvArray->ft->getSimpleType(rh.rvArray,NULL)==CMPI_ref) {
      CMPIEnumeration *enm=newCMPIEnumeration(rh.rvArray,NULL);
      CMSetStatus(rc,CMPI_RC_OK);
      return enm;
   }   
   
   CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,"Unexpected return value");
   return NULL;
}



static CMPIInstance* getInstance(CMCIClient* mb,
    CMPIObjectPath* cop, char** properties, CMPIStatus* rc)
{
   ClientEnc *cl=(ClientEnc*)mb;
   CMCIConnection *con=cl->connection;
   UtilStringBuffer *sb=newStringBuffer(2048);
   UtilList *nsc;
   CMPIString *cn;
   char *ns,*error;
   
   cn=cop->ft->getClassName(cop,NULL);
   con->ft->genRequest(cl,"GetInstance",cop,0,0);
   sb->ft->appendChars(sb,xmlHeader);
   
   nsc=getNameSpaceComponents(cop);
   sb->ft->appendChars(sb,"<IMETHODCALL NAME=\"GetInstance\">\n"
          "<LOCALNAMESPACEPATH>");
   for (ns=nsc->ft->getFirst(nsc); ns; ns=nsc->ft->getNext(nsc))
      sb->ft->append3Chars(sb,"<NAMESPACE NAME=\"",ns,"\"></NAMESPACE>");
   sb->ft->appendChars(sb,"</LOCALNAMESPACEPATH>\n");

   emitlocal(sb,0);
   emitorigin(sb,0);
   emitqual(sb,0);

   if (properties) {
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"PropertyList\"><VALUE.ARRAY>");
      while (*properties) {
          sb->ft->append3Chars(sb,"<VALUE>",*properties,"</VALUE>");
          properties++;
      }    
      sb->ft->appendChars(sb,"</VALUE.ARRAY></IPARAMVALUE>\n");
   }

   sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"InstanceName\">\n"
          "<INSTANCENAME CLASSNAME=\"",(char*)cn->hdl,"\">\n");

   pathToXml(sb, cop);

   sb->ft->appendChars(sb,"</INSTANCENAME>\n</IPARAMVALUE>\n");

   sb->ft->appendChars(sb,"</IMETHODCALL></SIMPLEREQ>\n</MESSAGE></CIM>");

//   fprintf(stderr,"%s\n",sb->ft->getCharPtr(sb));
   con->ft->addPayload(con,sb);
   
   if ((error=con->ft->getResponse(con,cop))) {
      CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,error);
      return NULL;
   }
//   fprintf(stderr,"%s\n",con->mResponse->ft->getCharPtr(con->mResponse));
   
   ResponseHdr rh=scanCimXmlResponse(con->mResponse->ft->getCharPtr(con->mResponse),cop);

   if (rh.errCode!=0) {
      CMSetStatusWithChars(rc,rh.errCode,rh.description);
      return NULL;
   }
   
   if (rh.rvArray->ft->getSimpleType(rh.rvArray,NULL)==CMPI_instance) {
      CMSetStatus(rc,CMPI_RC_OK);
      return rh.rvArray->ft->getElementAt(rh.rvArray,0,NULL).value.inst;
   }   
   
   CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,"Unexpected return value");
   return NULL;
}    
     


static CMPIEnumeration* execQuery (CMCIClient* cl,
                 CMPIObjectPath* op, const char *query, const char *lang, CMPIStatus* rc)
{    
   CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,"Not implemented by sfcc");
   return NULL;
}                 



static CMPIEnumeration* associators (CMCIClient* mb,
          CMPIObjectPath* cop, const char *assocClass, const char *resultClass,
          const char *role, const char *resultRole, char** properties, CMPIStatus* rc)
    
{
   ClientEnc *cl=(ClientEnc*)mb;
   CMCIConnection *con=cl->connection;
   UtilStringBuffer *sb=newStringBuffer(2048);
   UtilList *nsc;
   CMPIString *cn;
   char *ns,*error;
   
   cn=cop->ft->getClassName(cop,NULL);
   con->ft->genRequest(cl,"Associators",cop,0,0);
   sb->ft->appendChars(sb,xmlHeader);
   
   nsc=getNameSpaceComponents(cop);
   sb->ft->appendChars(sb,"<IMETHODCALL NAME=\"Associators\">\n"
          "<LOCALNAMESPACEPATH>");
   for (ns=nsc->ft->getFirst(nsc); ns; ns=nsc->ft->getNext(nsc))
      sb->ft->append3Chars(sb,"<NAMESPACE NAME=\"",ns,"\"></NAMESPACE>");
   sb->ft->appendChars(sb,"</LOCALNAMESPACEPATH>\n");
   
   sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"ObjectName\">\n"
          "<INSTANCENAME CLASSNAME=\"",(char*)cn->hdl,"\">\n");
   pathToXml(sb, cop);
   sb->ft->appendChars(sb,"</INSTANCENAME>\n</IPARAMVALUE>\n");

   if (assocClass!=NULL)
      sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"AssocClass\"><CLASSNAME NAME=\"", assocClass,
          "\"/></IPARAMVALUE>\n");
   if (resultClass!=NULL)
      sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"ResultClass\"><CLASSNAME NAME=\"", resultClass,
          "\"/></IPARAMVALUE>\n");

   if (role)
      sb->ft->append3Chars(sb, "<IPARAMVALUE NAME=\"Role\"><VALUE>", role,
         "</VALUE></IPARAMVALUE>\n");
   if (resultRole)
      sb->ft->append3Chars(sb, "<IPARAMVALUE NAME=\"ResultRole\"><VALUE>", resultRole,
         "</VALUE></IPARAMVALUE>\n");
   
   emitorigin(sb,0);
   emitqual(sb,0);

   if (properties) {
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"PropertyList\"><VALUE.ARRAY>");
      while (*properties) {
          sb->ft->append3Chars(sb,"<VALUE>",*properties,"</VALUE>");
          properties++;
      }    
      sb->ft->appendChars(sb,"</VALUE.ARRAY></IPARAMVALUE>\n");
   }

   sb->ft->appendChars(sb,"</IMETHODCALL></SIMPLEREQ>\n</MESSAGE></CIM>");

//   fprintf(stderr,"%s\n",sb->ft->getCharPtr(sb));
   con->ft->addPayload(con,sb);
   
   if ((error=con->ft->getResponse(con,cop))) {
      CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,error);
      return NULL;
   }
//   fprintf(stderr,"%s\n",con->mResponse->ft->getCharPtr(con->mResponse));
   
   ResponseHdr rh=scanCimXmlResponse(con->mResponse->ft->getCharPtr(con->mResponse),cop);

   if (rh.errCode!=0) {
      CMSetStatusWithChars(rc,rh.errCode,rh.description);
      return NULL;
   }
   
   if (rh.rvArray->ft->getSimpleType(rh.rvArray,NULL)==CMPI_instance) {
      CMPIEnumeration *enm=newCMPIEnumeration(rh.rvArray,NULL);
      CMSetStatus(rc,CMPI_RC_OK);
      return enm;
   }   
   
   CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,"Unexpected return value");
   return NULL;
}

static CMPIEnumeration* associatorNames(CMCIClient* mb,
       CMPIObjectPath* cop, const char *assocClass, const char *resultClass,
       const char *role, const char *resultRole, CMPIStatus* rc)
{
   ClientEnc *cl=(ClientEnc*)mb;
   CMCIConnection *con=cl->connection;
   UtilStringBuffer *sb=newStringBuffer(2048);
   UtilList *nsc;
   CMPIString *cn;
   char *ns,*error;
   
   cn=cop->ft->getClassName(cop,NULL);
   con->ft->genRequest(cl,"AssociatorNames",cop,0,0);
   sb->ft->appendChars(sb,xmlHeader);
   
   nsc=getNameSpaceComponents(cop);
   sb->ft->appendChars(sb,"<IMETHODCALL NAME=\"AssociatorNames\">\n"
          "<LOCALNAMESPACEPATH>");
   for (ns=nsc->ft->getFirst(nsc); ns; ns=nsc->ft->getNext(nsc))
      sb->ft->append3Chars(sb,"<NAMESPACE NAME=\"",ns,"\"></NAMESPACE>");
   sb->ft->appendChars(sb,"</LOCALNAMESPACEPATH>\n");
   
   sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"ObjectName\">\n"
          "<INSTANCENAME CLASSNAME=\"",(char*)cn->hdl,"\">\n");
   pathToXml(sb, cop);
   sb->ft->appendChars(sb,"</INSTANCENAME>\n</IPARAMVALUE>\n");

   if (assocClass!=NULL)
      sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"AssocClass\"><CLASSNAME NAME=\"", assocClass,
          "\"/></IPARAMVALUE>\n");
   if (resultClass!=NULL)
      sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"ResultClass\"><CLASSNAME NAME=\"", resultClass,
          "\"/></IPARAMVALUE>\n");

   if (role)
      sb->ft->append3Chars(sb, "<IPARAMVALUE NAME=\"Role\"><VALUE>", role,
         "</VALUE></IPARAMVALUE>\n");
   if (resultRole)
      sb->ft->append3Chars(sb, "<IPARAMVALUE NAME=\"ResultRole\"><VALUE>", resultRole,
         "</VALUE></IPARAMVALUE>\n");
   
   sb->ft->appendChars(sb,"</IMETHODCALL></SIMPLEREQ>\n</MESSAGE></CIM>");
   
//   fprintf(stderr,"%s\n",sb->ft->getCharPtr(sb));
   con->ft->addPayload(con,sb);
   
   if ((error=con->ft->getResponse(con,cop))) {
      CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,error);
      return NULL;
   }
//   fprintf(stderr,"%s\n",con->mResponse->ft->getCharPtr(con->mResponse));
   
   ResponseHdr rh=scanCimXmlResponse(con->mResponse->ft->getCharPtr(con->mResponse),cop);

   if (rh.errCode!=0) {
      CMSetStatusWithChars(rc,rh.errCode,rh.description);
      return NULL;
   }
   
   if (rh.rvArray->ft->getSimpleType(rh.rvArray,NULL)==CMPI_ref) {
      CMPIEnumeration *enm=newCMPIEnumeration(rh.rvArray,NULL);
      CMSetStatus(rc,CMPI_RC_OK);
      return enm;
   }   
   
   CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,"Unexpected return value");
   return NULL;
}       



static CMPIEnumeration* references(CMCIClient* mb,
         CMPIObjectPath* cop, const char *resultClass ,const char *role ,
         char** properties, CMPIStatus* rc)
{         
   ClientEnc *cl=(ClientEnc*)mb;
   CMCIConnection *con=cl->connection;
   UtilStringBuffer *sb=newStringBuffer(2048);
   UtilList *nsc;
   CMPIString *cn;
   char *ns,*error;
   
   cn=cop->ft->getClassName(cop,NULL);
   con->ft->genRequest(cl,"References",cop,0,0);
   sb->ft->appendChars(sb,xmlHeader);
   
   nsc=getNameSpaceComponents(cop);
   sb->ft->appendChars(sb,"<IMETHODCALL NAME=\"References\">\n"
          "<LOCALNAMESPACEPATH>");
   for (ns=nsc->ft->getFirst(nsc); ns; ns=nsc->ft->getNext(nsc))
      sb->ft->append3Chars(sb,"<NAMESPACE NAME=\"",ns,"\"></NAMESPACE>");
   sb->ft->appendChars(sb,"</LOCALNAMESPACEPATH>\n");
   
   sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"ObjectName\">\n"
          "<INSTANCENAME CLASSNAME=\"",(char*)cn->hdl,"\">\n");
   pathToXml(sb, cop);
   sb->ft->appendChars(sb,"</INSTANCENAME>\n</IPARAMVALUE>\n");

   if (resultClass)
      sb->ft->append3Chars(sb, "<IPARAMVALUE NAME=\"ResultClass\"><VALUE>", resultClass,
         "</VALUE></IPARAMVALUE>\n");
   if (role)
      sb->ft->append3Chars(sb, "<IPARAMVALUE NAME=\"Role\"><VALUE>", role,
         "</VALUE></IPARAMVALUE>\n");
   
   emitorigin(sb,0);
   emitqual(sb,0);

   if (properties) {
      sb->ft->appendChars(sb,"<IPARAMVALUE NAME=\"PropertyList\"><VALUE.ARRAY>");
      while (*properties) {
          sb->ft->append3Chars(sb,"<VALUE>",*properties,"</VALUE>");
          properties++;
      }    
      sb->ft->appendChars(sb,"</VALUE.ARRAY></IPARAMVALUE>\n");
   }

   sb->ft->appendChars(sb,"</IMETHODCALL></SIMPLEREQ>\n</MESSAGE></CIM>");

//   fprintf(stderr,"%s\n",sb->ft->getCharPtr(sb));
   con->ft->addPayload(con,sb);
   
   if ((error=con->ft->getResponse(con,cop))) {
      CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,error);
      return NULL;
   }
//   fprintf(stderr,"%s\n",con->mResponse->ft->getCharPtr(con->mResponse));
   
   ResponseHdr rh=scanCimXmlResponse(con->mResponse->ft->getCharPtr(con->mResponse),cop);

   if (rh.errCode!=0) {
      CMSetStatusWithChars(rc,rh.errCode,rh.description);
      return NULL;
   }
   
   if (rh.rvArray->ft->getSimpleType(rh.rvArray,NULL)==CMPI_instance) {
      CMPIEnumeration *enm=newCMPIEnumeration(rh.rvArray,NULL);
      CMSetStatus(rc,CMPI_RC_OK);
      return enm;
   }   
   
   CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,"Unexpected return value");
   return NULL;
}



static CMPIEnumeration* referenceNames(CMCIClient* mb,
                 CMPIObjectPath* cop, const char *resultClass ,const char *role,
                 CMPIStatus* rc)
{
   ClientEnc *cl=(ClientEnc*)mb;
   CMCIConnection *con=cl->connection;
   UtilStringBuffer *sb=newStringBuffer(2048);
   UtilList *nsc;
   CMPIString *cn;
   char *ns,*error;
   
   cn=cop->ft->getClassName(cop,NULL);
   con->ft->genRequest(cl,"ReferenceNames",cop,0,0);
   sb->ft->appendChars(sb,xmlHeader);
   
   nsc=getNameSpaceComponents(cop);
   sb->ft->appendChars(sb,"<IMETHODCALL NAME=\"ReferenceNames\">\n"
          "<LOCALNAMESPACEPATH>");
   for (ns=nsc->ft->getFirst(nsc); ns; ns=nsc->ft->getNext(nsc))
      sb->ft->append3Chars(sb,"<NAMESPACE NAME=\"",ns,"\"></NAMESPACE>");
   sb->ft->appendChars(sb,"</LOCALNAMESPACEPATH>\n");
   
   sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"ObjectName\">\n"
          "<INSTANCENAME CLASSNAME=\"",(char*)cn->hdl,"\">\n");
   pathToXml(sb, cop);
   sb->ft->appendChars(sb,"</INSTANCENAME>\n</IPARAMVALUE>\n");

   if (resultClass!=NULL)
      sb->ft->append3Chars(sb,"<IPARAMVALUE NAME=\"ResultClass\"><CLASSNAME NAME=\"", resultClass,
          "\"/></IPARAMVALUE>\n");
   if (role)
      sb->ft->append3Chars(sb, "<IPARAMVALUE NAME=\"Role\"><VALUE>", role,
         "</VALUE></IPARAMVALUE>\n");
   
   sb->ft->appendChars(sb,"</IMETHODCALL></SIMPLEREQ>\n</MESSAGE></CIM>");
   
//   fprintf(stderr,"%s\n",sb->ft->getCharPtr(sb));
   con->ft->addPayload(con,sb);
   
   if ((error=con->ft->getResponse(con,cop))) {
      CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,error);
      return NULL;
   }
//   fprintf(stderr,"%s\n",con->mResponse->ft->getCharPtr(con->mResponse));
   
   ResponseHdr rh=scanCimXmlResponse(con->mResponse->ft->getCharPtr(con->mResponse),cop);

   if (rh.errCode!=0) {
      CMSetStatusWithChars(rc,rh.errCode,rh.description);
      return NULL;
   }
   
   if (rh.rvArray->ft->getSimpleType(rh.rvArray,NULL)==CMPI_ref) {
      CMPIEnumeration *enm=newCMPIEnumeration(rh.rvArray,NULL);
      CMSetStatus(rc,CMPI_RC_OK);
      return enm;
   }   
   
   CMSetStatusWithChars(rc,CMPI_RC_ERR_FAILED,"Unexpected return value");
   return NULL;
}                 






static CMCIClientFT clientFt = { 
   enumInstanceNames,
   getInstance,
   NULL, // createInstance
   NULL, // setInstance,
   NULL, // deleteInstance,
   execQuery,
   NULL, // enumInstances,
   associators,
   associatorNames,
   references,
   referenceNames,
   NULL, // invokeMethod,
   NULL, // setProperty,
   NULL, // getProperty
};


CMCIClient *cmciConnect(const char *hn, const char *port, 
                        const char *user, const char *pwd, CMPIStatus *rc)
{
   
   ClientEnc *cc=(ClientEnc*)calloc(1,sizeof(ClientEnc));
   
   cc->enc.hdl=&cc->data;
   cc->enc.ft=&clientFt;
   
   cc->data.hostName=  hn ?   strdup(hn)   : "localhost";
   cc->data.port=      port ? strdup(port) : "5988";
   cc->data.user=      user ? strdup(user) : NULL;
   cc->data.pwd=       pwd ?  strdup(pwd)  : NULL;
   cc->data.scheme=    "http";
   
   cc->connection=initConnection(&cc->data);
   
   return (CMCIClient*)cc;
   
}                          


