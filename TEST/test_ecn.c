/*
 * test_ecn.c
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
 *  Test for enumClassNames() library API. Note that this test case
 *  requires that the CIM schema 2.10 final and the test instance MOF
 *  be installed in root/iicmv1. For more information see library README.
 */
#include <lib/cmci.h>
#include <lib/native.h>
#include <unistd.h>

#include "show.h"

int main( int argc, char * argv[] )
{
    CMCIClient *cc;
    CMPIObjectPath * objectpath;
    CMPIEnumeration * enumeration;
    CMPIStatus status;
    char hostName[512];

    /* Setup a conncetion to the CIMOM */   
    cc = cmciConnect("localhost", NULL, "5988", "root", "password", NULL);
   
    /* Test enumClassNames() */
    printf("\n----------------------------------------------------------\n");
    printf("Testing enumClassNames() ...\n");   
    objectpath = newCMPIObjectPath("root/iicmv1", NULL, NULL);
    enumeration = cc->ft->enumClassNames(cc, objectpath, 0, &status);

    /* Print the results */
    printf( "enumClassNames() rc=%d, msg=%s\n", status.rc, 
            (status.msg)? (char *)status.msg->hdl : NULL);

    if (!status.rc) {
        printf("result(s):\n");
        while (enumeration->ft->hasNext(enumeration, NULL)) {
            CMPIData data = enumeration->ft->getNext(enumeration, NULL);
            showObjectPath(data.value.ref);
        }
    }

    if (enumeration) CMRelease(enumeration);
    if (objectpath) CMRelease(objectpath);
    
    return 0;
}