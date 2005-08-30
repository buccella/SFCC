/*
 * show.c
 *
 * (C) Copyright IBM Corp. 2005
 * (C) Copyright Intel Corp. 2005
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
 *    Test case show functions for instance, object paths
 */
#include <lib/cmci.h>
#include <lib/native.h>
#include <unistd.h>

#include "show.h"

extern char *value2Chars(CMPIType type, CMPIValue * value);

void showObjectPath( CMPIObjectPath * objectpath )
{
   CMPIString * namespace = objectpath->ft->getNameSpace(objectpath, NULL);
   CMPIString * classname = objectpath->ft->getClassName(objectpath, NULL);
   int numkeys = objectpath->ft->getKeyCount(objectpath, NULL);
   int i;

   if (namespace && namespace->hdl) printf("namespace=%s\n", (char *)namespace->hdl);
   if (classname && classname->hdl) printf("classname=%s\n", (char *)classname->hdl);
   if (numkeys) {
      printf("keys:\n");
      for (i=0; i<numkeys; i++) {
         CMPIString * keyname;
         CMPIData data = objectpath->ft->getKeyAt(objectpath, i, &keyname, NULL);
         printf("\t%s=%s\n", (char *)keyname->hdl, value2Chars(data.type, &data.value)); 
      }
   }

   if (classname) CMRelease(classname); 
   if (namespace) CMRelease(namespace);
}


void showInstance( CMPIInstance *instance )
{
   CMPIObjectPath * objectpath = instance->ft->getObjectPath(instance, NULL);
   CMPIString * objectpathname = objectpath->ft->toString(objectpath, NULL);
   CMPIString * namespace = objectpath->ft->getNameSpace(objectpath, NULL);
   CMPIString * classname = objectpath->ft->getClassName(objectpath, NULL);
   int numkeys = objectpath->ft->getKeyCount(objectpath, NULL);
   int numproperties = instance->ft->getPropertyCount(instance, NULL);
   int i;

   if (objectpathname && objectpathname->hdl) printf("objectpath=%s\n", (char *)objectpathname->hdl);
   if (namespace && namespace->hdl) printf("namespace=%s\n", (char *)namespace->hdl);
   if (classname && classname->hdl) printf("classname=%s\n", (char *)classname->hdl);
   if (numkeys) {
      printf("keys:\n");
      for (i=0; i<numkeys; i++) {
         CMPIString * keyname;
         CMPIData data = objectpath->ft->getKeyAt(objectpath, i, &keyname, NULL);
         printf("\t%s=%s\n", (char *)keyname->hdl, value2Chars(data.type, &data.value)); 
      }
   }
   else
      printf("keys are NULL !\n");
   if (numproperties) {
      printf("properties:\n");
      for (i=0; i<numproperties; i++) {
         CMPIString * propertyname;
         CMPIData data = instance->ft->getPropertyAt(instance, i, &propertyname, NULL);
         printf("\t%s=%s\n", (char *)propertyname->hdl, value2Chars(data.type, &data.value));
      }
   }
   else
      printf("properties are NULL !\n");

   if (classname) CMRelease(classname);
   if (namespace) CMRelease(namespace);
   if (objectpathname) CMRelease(objectpathname);
   if (objectpath) CMRelease(objectpath);
}


void showClass( CMPIConstClass * class )
{
   CMPIString * classname = class->ft->getClassName(class, NULL);
   int numproperties = class->ft->getPropertyCount(class, NULL);
   int i;

   if (classname && classname->hdl) printf("classname=%s\n", (char *)classname->hdl);
   if (numproperties) {
      printf("properties:\n");
      for (i=0; i<numproperties; i++) {
         CMPIString * propertyname;
         CMPIData data = class->ft->getPropertyAt(class, i, &propertyname, NULL);
         if (data.state==0)
            printf("\t%s=%s\n", (char *)propertyname->hdl, value2Chars(data.type, &data.value));
         else printf("\t%s=NIL\n", (char *)propertyname->hdl);
      }
   }

   if (classname) CMRelease(classname);
}
