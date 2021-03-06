/*******************************************************************************
**
**  Copyright (c) 1999-2012 VisualSonics Inc. All Rights Reserved.
**
**  VsiCSVWriter.cpp
**
**	Description:
**		Implementation of CVsiCSVWriter
**
*******************************************************************************/

#include "stdafx.h"
#include <VsiBuildNumber.h>
#include <VsiSaxUtils.h>
#include <VsiCommUtlLib.h>
#include <atltime.h>
#include <ATLComTime.h>
#include <VsiAnalRsltXmlTags.h>
#include <VsiAnalysisResultsModule.h>
#include <VsiImportExportModule.h>
#include <VsiLicenseUtils.h>
#include <VsiMsmntHelper.h>
#include "VsiCSVWriter.h"


const DWORD g_pdwCustomId[] = {
	VSI_PROP_SERIES_CUSTOM1,
	VSI_PROP_SERIES_CUSTOM2,
	VSI_PROP_SERIES_CUSTOM3,
	VSI_PROP_SERIES_CUSTOM4,
	VSI_PROP_SERIES_CUSTOM5,
	VSI_PROP_SERIES_CUSTOM6,
	VSI_PROP_SERIES_CUSTOM7,
	VSI_PROP_SERIES_CUSTOM8,
	VSI_PROP_SERIES_CUSTOM9,
	VSI_PROP_SERIES_CUSTOM10,
	VSI_PROP_SERIES_CUSTOM11,
	VSI_PROP_SERIES_CUSTOM12,
	VSI_PROP_SERIES_CUSTOM13,
	VSI_PROP_SERIES_CUSTOM14,
};

struct report_order :
	public std::binary_function<CString&,CString&,bool> 
	{	// functor for operator<
		report_order(long s_) : m_lAscending(s_) {};
		bool operator()(const CString& _Left, const CString& _Right) const
			{	// apply operator< to operands
				int iCmp = lstrcmp(_Left, _Right);
				if (!m_lAscending)
					iCmp *= -1;
				return (iCmp < 0);
			}
		long m_lAscending;
	};

/// <Summary>
///	Get the character to be used as the field separator based on User Locale
/// </Summary>
/// <Returns>int : </Returns>
HRESULT CVsiCSVWriter::GetFieldSeparator(LPWSTR pszSptr, int iCount)
{
	HRESULT hr = S_OK;

	try
	{
		WCHAR szDecimalSep[8];
		int iRet = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, szDecimalSep, _countof(szDecimalSep));
		VSI_WIN32_VERIFY((0 != iRet), VSI_LOG_ERROR, L"Failed to get field separator");

		if (0 != lstrcmp(szDecimalSep, L","))
		{
			wcscpy_s(pszSptr, iCount, L",");
		}
		else
		{
			wcscpy_s(pszSptr, iCount, L";");
		}
	}
	VSI_CATCH(hr);

	return hr;
}


HRESULT CVsiCSVWriter::GetFieldSeparator()
{
	return GetFieldSeparator(m_szSptr, _countof(m_szSptr));
}

HRESULT CVsiCSVWriter::WriteCSVFile(HANDLE hFile, CString& strOut)
{
	HRESULT hr = S_OK;

	try
	{
		hr = VsiMsmntHelper::VsiReplaceSpecialCharacters(strOut);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to replace special characters");

		CW2A out(strOut);		
		DWORD dwLen = strOut.GetLength();

		DWORD dwWritten;
		BOOL bRet = WriteFile(hFile, out, dwLen, &dwWritten, NULL); 
		VSI_WIN32_VERIFY(bRet, VSI_LOG_ERROR, L"Failure writing to CSV file"); 
	}
	VSI_CATCH(hr);
	
	return hr;
}

/// <Summary>
///		Convert the Header element from the intermediate XML to CSV
/// </Summary>
///
/// <Param name = "HANDLE hFile"></Param>
///
/// <Returns>HRESULT : </Returns>
HRESULT CVsiCSVWriter::ConvertHeaderSection(HANDLE hFile)
{
	HRESULT hr = S_OK;
	CString strOut;
	CString strTemp;

	try
	{
		WCHAR szReportDate[80];
		CComVariant vReportVersion;

		CComPtr<IXMLDOMNode> pAnalysisNode;
		hr = m_pDoc->selectSingleNode(CComBSTR(VSI_MSMNT_XML_ELE_ANAL_RSLTS), &pAnalysisNode);
		VSI_VERIFY(S_OK == hr, VSI_LOG_ERROR, L"Failed to get header tag");

		// Report
		{
			// Get current date - it will appear in the report
			SYSTEMTIME stDate;
			COleDateTime date = COleDateTime::GetCurrentTime();
			date.GetAsSystemTime(stDate);

			// Convert it to local time for display.
			SystemTimeToTzSpecificLocalTime(NULL, &stDate, &stDate);

			GetDateFormat(
				LOCALE_USER_DEFAULT,
				DATE_SHORTDATE,
				&stDate,
				NULL,
				szReportDate,
				_countof(szReportDate));
			
			CComQIPtr<IXMLDOMElement> pAnalysisElement(pAnalysisNode);
			VSI_CHECK_INTERFACE(pAnalysisElement, VSI_LOG_ERROR, NULL);

			// Get version 
			hr = pAnalysisElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_VERSION), &vReportVersion);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
		}

		strOut = L"\"VisualSonics® Measurement Export\"\r\n";

		// Add the institution name to the report if it is defined.
		{	
			WCHAR szCompanyName[1024];

			CVsiRange<LPCWSTR> pInstitutionParam;
			hr = m_pPdm->GetParameter(
				VSI_PDM_ROOT_APP, VSI_PDM_GROUP_SETTINGS, VSI_PARAMETER_SYS_INSTITUTION,
				&pInstitutionParam);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

			CComQIPtr<IVsiParameterRange> pRange(pInstitutionParam.m_pParam);
			VSI_CHECK_INTERFACE(pRange, VSI_LOG_ERROR, NULL);

			CComVariant vInstitutionName;
			hr = pRange->GetValue(&vInstitutionName);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

			wcscpy_s(szCompanyName, _countof(szCompanyName), V_BSTR(&vInstitutionName));

			strTemp.Format(L"\"Institution\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(szCompanyName));
			strOut += strTemp;
		}

		// Add report date to file.
		strTemp.Format(L"\"Report Date\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(szReportDate));
		strOut += strTemp;
		strOut += L"\r\n";

		strOut += L"\"Versioning Information\"\r\n";

		strTemp.Format(L"\"Company Name\"%s\"Model\"%s\"SW Ver\"%s\"SW Build\"%s\"Rept Ver\"\r\n", m_szSptr, m_szSptr, m_szSptr, m_szSptr);
		strOut += strTemp;

		// This needs to be product specific
		CString strProduct = VsiGetRangeValue<CString>(
			VSI_PDM_ROOT_APP, VSI_PDM_GROUP_GLOBAL,
			VSI_PARAMETER_SYS_PRODUCT_NAME,
			m_pPdm);
		
		strTemp.Format(L"\"VisualSonics®\"%s\"%s\"%s\"%i.%i.%i\"%s\"%i\"%s\"%s\"\r\n",
			m_szSptr, strProduct, m_szSptr,
			VSI_SOFTWARE_MAJOR, VSI_SOFTWARE_MIDDLE, VSI_SOFTWARE_MINOR,
			m_szSptr, VSI_BUILD_NUMBER, m_szSptr, V_BSTR(&vReportVersion));
		strOut += strTemp;
		strOut += L"\r\n";
		
		// Output an ID number for each study in the report.
		BuildStudiesList();

		// Convert and write output
		hr = WriteCSVFile(hFile, strOut); 
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failure writing to CSV file");
	}
	VSI_CATCH(hr);

	return hr;
}

/// <summary>
///		Given the string representing the study ID, find the index for the study
/// </summary>
///
/// <param name = "WCHAR *pszStudyID">Study ID as a string</param>
///
/// <returns>int : </returns>
int CVsiCSVWriter::GetStudyIndex(WCHAR *pszStudyID)
{
	int iIndex(0);

	STUDY_ID_LIST_ITER iter = m_StudyIDList.begin();
	for (; iter != m_StudyIDList.end(); ++iter)
	{
		if (0 == lstrcmpi(pszStudyID, iter->m_szStudyID))
			return iIndex;

		++iIndex;
	}

	return -1;	// Not found
}


/// <summary>
///	Count of the number of studies in the report
/// </summary>
/// <returns>int : </returns>
int CVsiCSVWriter::GetNumberOfStudies()
{
	return (int)m_StudyIDList.size();
}

/// <summary>
///	Spin through the Studies tags in the intermediate file and build a list of
///	the studies included in the report.
/// </summary>
/// <param name = "HANDLE hFile"></param>
/// <returns>long : </returns>
long CVsiCSVWriter::BuildStudiesList()
{
	long lNumStudies(0);
	CComVariant vStudyID;
	HRESULT hr = S_OK;

	try
	{
		CComPtr<IXMLDOMNodeList> pStudyList;
		hr = m_pDoc->getElementsByTagName(VSI_MSMNT_REPORT_XML_ELE_STUDY, &pStudyList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get study list");

		hr = pStudyList->get_length(&lNumStudies);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get number of studies");

		for (long lStudyIndex = 0; lStudyIndex < lNumStudies; ++lStudyIndex)
		{
			CComPtr<IXMLDOMNode> pStudyNode;
			hr = pStudyList->get_item(lStudyIndex, &pStudyNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get study node");

			CComQIPtr<IXMLDOMElement> pStudyElement(pStudyNode);
			VSI_CHECK_INTERFACE(pStudyElement, VSI_LOG_ERROR, L"Failed to get study element");

			VSI_STUDY_ID studyID;
			studyID.m_lIndex = lStudyIndex;

			vStudyID.Clear();
			hr = pStudyElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_STUDY_ID), &vStudyID);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get study ID");

			wcscpy_s(studyID.m_szStudyID, _countof(studyID.m_szStudyID), V_BSTR(&vStudyID));

			m_StudyIDList.push_back(studyID);
		}
	}
	VSI_CATCH(hr);

	return lNumStudies;
}

/// <summary>
///		Convert study to CSV
/// </summary>
///
/// <param name = "HANDLE hFile">Pointer to ouput file</param>
/// <param name = "IXMLDOMElement *pStudyElement">Element containing study information</param>
///
/// <returns>HRESULT : </returns>
HRESULT CVsiCSVWriter::ConvertStudy(HANDLE hFile, IXMLDOMElement *pStudyElement, BOOL bVevo2100)
{
	HRESULT hr = S_OK;

	try
	{
		CString strOut;

		// Study index
		CComVariant vStudyId;
		hr = pStudyElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_STUDY_ID), &vStudyId);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read study id");

		CString strTemp;
		strTemp.Format(L"\"%s\"%s\"%i\"\r\n", CString(MAKEINTRESOURCE(IDS_CSV_STUDY)), m_szSptr, GetStudyIndex(V_BSTR(&vStudyId)) + 1);
		strOut += strTemp;

		// Study name
		CComVariant vName;
		hr = pStudyElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_STUDY_NAME), &vName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read study name");

		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_STUDY_NAME)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vName)));
		strOut += strTemp;

		// Owner
		CComVariant vOwner;
		hr = pStudyElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_STUDY_OWNER), &vOwner);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_OWNER_OPERATOR_NAME)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vOwner)));
		strOut += strTemp;

		// Institution
		CComVariant vInstitution;
		hr = pStudyElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_STUDY_INSTITUTION), &vInstitution);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_GRANTING_INSTITUTION)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vInstitution)));
		strOut += strTemp;

		// Study Date
		CComVariant vDate;
		hr = pStudyElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_STUDY_DATE), &vDate);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_STUDY_DATE)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vDate)));
		strOut += strTemp;

		// Study Time
		CComVariant vTime;
		hr = pStudyElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_STUDY_TIME), &vTime);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		// Use %s here, %s doesn't work for many locals
		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_STUDY_TIME)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vTime)));
		strOut += strTemp;

		// Notes
		CComVariant vNotes;
		hr = pStudyElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_STUDY_NOTES), &vNotes);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_STUDY_NOTES)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vNotes)));
		strOut += strTemp;

		strOut += L"\r\n\r\n";

		// Convert and write output
		hr = WriteCSVFile(hFile, strOut); 
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failure writing to CSV file");

		// Convert series
		CComPtr<IXMLDOMNodeList> pSeriesNodeList;
		hr = pStudyElement->getElementsByTagName(VSI_MSMNT_REPORT_XML_ELE_SERIES, &pSeriesNodeList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		long lNumSeries(0);
		hr = pSeriesNodeList->get_length(&lNumSeries);
		for (int i = 0; i < lNumSeries; ++i)
		{
			CComPtr<IXMLDOMNode> pSeriesNode;
			hr = pSeriesNodeList->get_item(i, &pSeriesNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get series node");

			CComQIPtr<IXMLDOMElement> pSeriesElement(pSeriesNode);
			VSI_CHECK_PTR(pSeriesElement, VSI_LOG_ERROR, L"Failed to get series element");

			hr = ConvertSeries(hFile, pSeriesElement, bVevo2100);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
		}

		strOut = L"\r\n\r\n";

		// Convert and write output
		hr = WriteCSVFile(hFile, strOut); 
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failure writing to CSV file");
	}
	VSI_CATCH(hr);

	return hr;
}

/// <summary>
///	Convert series to CSV
/// </summary>
/// <param name = "HANDLE hFile">Pointer to output file</param>
/// <param name = "IXMLDOMElement *pSeriesElement">Element containing series information</param>
/// <returns>HRESULT : </returns>
HRESULT CVsiCSVWriter::ConvertSeries(HANDLE hFile, IXMLDOMElement *pSeriesElement, BOOL bVevo2100)
{
	HRESULT hr = S_OK;
	CComVariant vPkgId;
	int iPkgId;

	try
	{
		CString strOut;

		// Series name
		CComVariant vName;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_NAME), &vName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read series name");

		CString strTemp;
		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_SERIES_NAME)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vName)));
		strOut += strTemp;

		// Acquired By
		CComVariant vAcquiredBy;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_ACQUIRED_BY), &vAcquiredBy);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_ACQUIRED_BY)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vAcquiredBy)));
		strOut += strTemp;

		// Series created Date
		CComVariant vDate;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_DATE), &vDate);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_SERIES_DATE)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vDate)));
		strOut += strTemp;

		// Series created Time
		CComVariant vTime;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_TIME), &vTime);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		// Use %s here, %s doesn't work for many locals
		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_SERIES_TIME)), m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vTime)));
		strOut += strTemp;

		// Animal Id
		CComVariant vAnimalID;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_ANIMAL_ID), &vAnimalID);

		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_ANIMAL_ID)), m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vAnimalID) : L""));
		strOut += strTemp;

		// Animal Sex
		CComVariant vAnimalSex;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_SEX), &vAnimalSex);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to write id");

		m_bFemale = FALSE;
		strTemp.Format(L"\"Sex\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vAnimalSex) : L""));

		if (S_OK == hr)
		{
			if (lstrcmpi(V_BSTR(&vAnimalSex), L"female") == 0)
				m_bFemale = TRUE;
			else
				m_bFemale = FALSE;

		}
		strOut += strTemp;

		// Pregnant (not in VevoCI
		if (bVevo2100)
		{
			CComVariant vPregnant;
			hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_PREGNANT), &vPregnant);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to write id");

			m_bPregnant = FALSE;
			if (S_OK == hr)
			{
				strTemp.Format(L"\"Pregnant\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vPregnant)));
				strOut += strTemp;

				if (lstrcmpi(V_BSTR(&vPregnant), L"yes") == 0)
				{
					m_bPregnant = TRUE;

					// Date mated
					CComVariant vMated;
					hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_DATE_MATED), &vMated);
					strTemp.Format(L"\"Date Mated\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vMated) : L""));
					strOut += strTemp;

					// Date plugged
					CComVariant vPlugged;
					hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_DATE_PLUGGED), &vPlugged);
					strTemp.Format(L"\"Date Plugged\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vPlugged) : L""));
					strOut += strTemp;
				}
				else
				{
					m_bPregnant = FALSE;
				}
			}
		}

		// Series notes
		CComVariant vNotes;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_NOTES), &vNotes);

		strTemp.Format(L"\"%s\"%s%s\r\n", CString(MAKEINTRESOURCE(IDS_CSV_SERIES_NOTES)), m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vNotes) : L""));
		strOut += strTemp;

		// Series application
		CComVariant vApplication;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_APPLICATION), &vApplication);
		strTemp.Format(L"\"Application\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vApplication) : L""));
		strOut += strTemp;

		// Series package
		CComVariant vPackage;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_PACKAGE), &vPackage);
		strTemp.Format(L"\"Measurement Package\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vPackage) : L""));
		strOut += strTemp;

		// Series color
		if (bVevo2100)
		{
			CComVariant vColor;
			hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_COLOR), &vColor);
			strTemp.Format(L"\"Color\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vColor) : L""));
			strOut += strTemp;
		}

		// Series strain
		if (bVevo2100)
		{
			CComVariant vStrain;
			hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_STRAIN), &vStrain);
			strTemp.Format(L"\"Strain\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vStrain) : L""));
			strOut += strTemp;
		}

		// Series source
		if (bVevo2100)
		{
			CComVariant vSource;
			hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_SOURCE), &vSource);
			strTemp.Format(L"\"Source\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vSource) : L""));
			strOut += strTemp;
		}

		// Series weight
		if (bVevo2100)
		{
			CComVariant vWeight;
			hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_WEIGHT), &vWeight);
			strTemp.Format(L"\"Weight\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vWeight) : L""));
			strOut += strTemp;
		}

		// Series type
		if (bVevo2100)
		{
			CComVariant vType;
			hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_TYPE), &vType);
			strTemp.Format(L"\"Type\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vType) : L""));
			strOut += strTemp;
		}

		// Series date of birth
		CComVariant vDOB;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_DOB), &vDOB);
		strTemp.Format(L"\"Date of birth\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vDOB) : L""));
		strOut += strTemp;

		// Series body temp
		if (bVevo2100)
		{
			CComVariant vBodyTemp;
			hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_TEMP), &vBodyTemp);
			strTemp.Format(L"\"Temperature\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vBodyTemp) : L""));
			strOut += strTemp;
		}

		// Series heart rate
		if (bVevo2100)
		{
			CComVariant vHeartRate;
			hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_HEART_RATE), &vHeartRate);
			strTemp.Format(L"\"Heart rate\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(S_OK == hr ? V_BSTR(&vHeartRate) : L""));
			strOut += strTemp;
		}

		// Custom fields
		if (bVevo2100)
		{
			CString strTmpLabel;
			CString strTmpValue;		
			for (int i = 0; i < _countof(g_pdwCustomId); i++)
			{
				strTmpLabel.Format(L"custLabel%d", i);
				CComVariant vTmpLabel;
				hr = pSeriesElement->getAttribute(CComBSTR(strTmpLabel), &vTmpLabel);
				if (S_OK == hr)
				{
					strTmpValue.Format(L"custValue%d", i);
					CComVariant vTmpValue;
					hr = pSeriesElement->getAttribute(CComBSTR(strTmpValue), &vTmpValue);
					if (S_OK == hr)
					{
						strTemp.Format(
							L"%s%s%s\r\n",
							(LPCWSTR)FormatStringForCSV(V_BSTR(&vTmpLabel)),
							m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vTmpValue)));
						strOut += strTemp;
					}
				}
			}
		}

		strOut += L"\r\n\r\n";

		// Convert packages
		CComPtr<IXMLDOMNodeList> pPkgNodeList;
		hr = pSeriesElement->getElementsByTagName(VSI_MSMNT_XML_ELE_MSMNT_PKG, &pPkgNodeList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		long lNumPkg(0);
		hr = pPkgNodeList->get_length(&lNumPkg);
		for (int i = 0; i < lNumPkg; ++i)
		{
			CComPtr<IXMLDOMNode> pPkgNode;
			hr = pPkgNodeList->get_item(i, &pPkgNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get package node");

			CComQIPtr<IXMLDOMElement> pPkgElement(pPkgNode);
			VSI_CHECK_PTR(pPkgElement, VSI_LOG_ERROR, L"Failed to get package element");

			// Get package Id
			hr = pPkgElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PACKAGE_ID), &vPkgId);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
			hr = VariantChangeTypeEx(
				&vPkgId, &vPkgId,
				MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
				0, VT_I4);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

			iPkgId = V_I4(&vPkgId);

			if (0 == iPkgId)
			{
				if (VSI_CSV_WRITER_FILTER_MEASUREMENTS & m_filter)
				{
					strOut += L"\r\n";

					// Measurement File
					hr = ConvertPackageInfo(strOut, pPkgElement);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

					hr = ConvertProtocolMeasurements(strOut, pPkgElement, TRUE);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to convert generic package");
				}
			}
			else
			{
				hr = ConvertProtocolSection(strOut, pPkgElement);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, VsiFormatMsg(L"Failed to convert package %i", iPkgId));
			}
		}

		m_mapImages.clear();

		if (VSI_CSV_WRITER_FILTER_MEASUREMENTS & m_filter)
		{
			if (0 == lNumPkg)
			{
				ReportNoMsmnts(strOut);
			}
		}

		{
			// Display curves
			CComPtr<IXMLDOMNodeList> pCurveNodeList;
			hr = pSeriesElement->selectNodes(CComBSTR(VSI_MSMNT_ELE_CURVES L"/" VSI_MSMNT_ELE_CURVE), &pCurveNodeList);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

			long lNumCurve(0);
			hr = pCurveNodeList->get_length(&lNumCurve);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get curve elements");

			if (lNumCurve > 0)
			{
				strOut += L"\r\n\r\n";
				strOut += "\"Graphs\"\r\n\r\n";

				strTemp.Format(L"\"Name\"%s\"Description\"", m_szSptr);
				strOut += strTemp;
				strOut += L"\r\n";

				hr = pCurveNodeList->get_length(&lNumCurve);

				long lAscending = VsiGetDiscreteValue<long>(
					VSI_PDM_ROOT_APP, VSI_PDM_GROUP_GLOBAL,
					VSI_PARAMETER_UI_SORT_ORDER_ANALYSISBROWSER, m_pPdm);

				// Create ordering map here. map labels to indexes to save memory. 
				// Use indexes from ordered map to retrieve elements used for export
				report_order mo(lAscending);
				std::map<CString, int, report_order> mapGraphs(mo);
				for (long lGraphIndex = 0; lGraphIndex < lNumCurve; ++lGraphIndex)
				{
					CComPtr<IXMLDOMNode> pCurveNode;
					hr = pCurveNodeList->get_item(lGraphIndex, &pCurveNode);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get curve node");

					CComQIPtr<IXMLDOMElement> pCurveElement(pCurveNode);
					VSI_CHECK_INTERFACE(pCurveElement, VSI_LOG_ERROR, L"Failed to get curve element");

					// Get the measurement name
					vName.Clear();
					hr = pCurveElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read measurement name");

					CString strKey(V_BSTR(&vName));
					if (mapGraphs.find(strKey) != mapGraphs.end())
					{
						strKey.Format(L"%s%d", strKey, lGraphIndex);
					}

					mapGraphs.insert(std::map<CString, int>::value_type(strKey, lGraphIndex));
				}

				std::map<CString, int, report_order>::iterator itrr;
				for (itrr = mapGraphs.begin(); itrr != mapGraphs.end(); ++itrr)
				{
					CComPtr<IXMLDOMNode> pCurveNode;
					hr = pCurveNodeList->get_item((*itrr).second, &pCurveNode);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get curve node");

					CComQIPtr<IXMLDOMElement> pCurveElement(pCurveNode);
					VSI_CHECK_INTERFACE(pCurveElement, VSI_LOG_ERROR, L"Failed to get curve element");

					// Get curve name
					CComVariant vName;
					hr = pCurveElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read curve name");

					// Get curve type
					CComVariant vType;
					hr = pCurveElement->getAttribute(CComBSTR(VSI_MSMNT_XML_CURVE_TYPE), &vType);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read curve type");

					strTemp.Format(L"\"%s\"%s\"%s\"%s", V_BSTR(&vName), m_szSptr, V_BSTR(&vType), m_szSptr);
					strOut += strTemp;
					strOut += L"\r\n\r\n";

					CComPtr<IXMLDOMNodeList> pLoopNodeList;
					hr = pCurveElement->selectNodes(CComBSTR(VSI_MSMNT_XML_ELE_LOOP), &pLoopNodeList);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

					long lNumLoops;
					hr = pLoopNodeList->get_length(&lNumLoops);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

					CComVariant vId;
					//create ordering map here. map labels to indexes to save memory. 
					//use indexes from ordered map to retrieve elements used for export
					std::map<CString, int, report_order> mapCurves(mo);
					for (long lCurveIndex = 0; lCurveIndex < lNumLoops; ++lCurveIndex)
					{
						CComPtr<IXMLDOMNode> pLoopNode;
						hr = pLoopNodeList->get_item(lCurveIndex, &pLoopNode);
						VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get loop node");

						CComQIPtr<IXMLDOMElement> pLoopElement(pLoopNode);
						VSI_CHECK_INTERFACE(pLoopElement, VSI_LOG_ERROR, L"Failed to get loop element");

						// Get the measurement name
						vId.Clear();
						hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_MSMNT_ID), &vId);
						VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read measurement name");

						mapCurves.insert(std::map<CString, int>::value_type(CString(V_BSTR(&vId)), lCurveIndex));
					}

					std::map<CString, int, report_order>::iterator itr;
					for (itr = mapCurves.begin(); itr != mapCurves.end(); ++itr)
					{
						CComPtr<IXMLDOMNode> pLoopNode;
						hr = pLoopNodeList->get_item((*itr).second, &pLoopNode);
						VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get loop node");

						CComQIPtr<IXMLDOMElement> pLoopElement(pLoopNode);
						VSI_CHECK_INTERFACE(pLoopElement, VSI_LOG_ERROR, L"Failed to get loop element");

						hr = ExportMsmntData(strOut, pLoopElement);

#pragma MESSAGE(L"TODO: deal with backward compatibility here...")
						// If we failed to export the new way, try to find the data the old way
						if (S_OK != hr)
						{
							if (0 == lstrcmp(VSI_ANALYSIS_CURVE_PRESSURE_VOLUME, V_BSTR(&vType)))
							{
								// Get series path to read PV data files
								CComVariant vSeriesPath;
								hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_PATH), &vSeriesPath);
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read seriesPath");

								CString csSeriesPath(V_BSTR(&vSeriesPath));
								PathRemoveFileSpec((LPWSTR)(LPCWSTR)csSeriesPath);  // Remove series file

								// Get typeID
								CComVariant vTypeID;
								hr = pCurveElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_TYPE_ID), &vTypeID);
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read curve type");
								hr = VariantChangeTypeEx(
									&vTypeID, &vTypeID,
									MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
									0, VT_UI4);
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read curve type");

								WCHAR szDimension[30];
								if (VSI_PV_DIMENSION_AREA == V_UI4(&vTypeID))
									wcscpy_s(szDimension, _countof(szDimension), L"Area (mm2)");
								else if (VSI_PV_DIMENSION_DEPTH == V_UI4(&vTypeID))
									wcscpy_s(szDimension, _countof(szDimension), L"Diameter (mm)");
								else if (VSI_PV_DIMENSION_VOLUME == V_UI4(&vTypeID))
									wcscpy_s(szDimension, _countof(szDimension), L"Volume (ul)");
								else
									wcscpy_s(szDimension, _countof(szDimension), L"Dimension");

								// Output loops
								{
									CComPtr<IXMLDOMNodeList> pLoopNodeList;
									hr = pCurveElement->selectNodes(CComBSTR(VSI_MSMNT_XML_ELE_LOOP), &pLoopNodeList);
									VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

									long lNumLoop(0);
									hr = pLoopNodeList->get_length(&lNumLoop);
									VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get loop elements");

									m_vPVInfo.clear();

									for(long j = 0; j < lNumLoop; ++j)
									{
										CComPtr<IXMLDOMNode> pLoopNode;
										hr = pLoopNodeList->get_item(j, &pLoopNode);
										VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get loop node");

										CComQIPtr<IXMLDOMElement> pLoopElement(pLoopNode);
										VSI_CHECK_INTERFACE(pLoopElement, VSI_LOG_ERROR, L"Failed to get loop element");

										// Get loop name
										vName.Clear();
										hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
										VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read curve name");

										// Get data file
										CComVariant vDataFile;
										hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_DATA_FILE), &vDataFile);
										VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read curve name");


										CComPtr<IVsiAnalPressureVolumeInfo> pPVInfo;
										hr = pPVInfo.CoCreateInstance(__uuidof(VsiAnalPressureVolumeInfo));
										VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

										hr = pPVInfo->Initialize(L"", L"", csSeriesPath, L"");
										VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

										// Read samples from the data file and add it to the PVInfo object
										CString csDataFile;
										csDataFile.Format(L"%s\\%s", csSeriesPath, V_BSTR(&vDataFile));
										hr = pPVInfo->ReadDataFile(csDataFile);
										VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

										m_vPVInfo.push_back(pPVInfo);

										strTemp.Format(L"%s%s%s%s%s%s%s", m_szSptr, V_BSTR(&vName), m_szSptr, m_szSptr, m_szSptr, m_szSptr, m_szSptr);
										strOut += strTemp;
									}

									strOut += L"\r\n";

									// Output column headings. If we are displaying an averaged loop then only one set of data
									// hence one set of heading. If not averaging then output one set of headings for each loop
									// in the set.
									strOut += "\"Sample\"";
									for(long j = 0; j < lNumLoop; ++j)
									{
										strTemp.Format(L"%s\"Time\"%s\"%s\"%s\"Volume (ul)\"%s\"Pressure\"%s\"Derivative\"%s\"ECG Amplitude\"",
														m_szSptr, m_szSptr, szDimension, m_szSptr, m_szSptr, m_szSptr, m_szSptr);
										strOut += strTemp;
									}
									strOut += L"\r\n";

									hr = ExportPVLoopData(strOut);
									VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

									strOut += L"\r\n\r\n";
								}

							}
							else if (0 == lstrcmp(VSI_ANALYSIS_CURVE_HISTOGRAM, V_BSTR(&vType)))
							{
								CString strSeparator(m_szSptr);

								ADD_SIMPLE_STRING_TO_STRING(L"Histogram Statistics");
								ADD_NEWLINE_TO_STRING();

								// Get mean value
								CComVariant vMean;
								hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_MEAN), &vMean);
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
								hr = VariantChangeTypeEx(
									&vMean, &vMean,
									MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
									0, VT_R8);
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

								// Get std value
								CComVariant vStd;
								hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_STD), &vStd);
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
								hr = VariantChangeTypeEx(
									&vStd, &vStd,
									MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
									0, VT_R8);
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

								// Get data
								CComVariant vData;
								hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_CURVE_DATA), &vData);
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

								CString strData = V_BSTR(&vData);

								// Read number of bins
								int iBinsNum(0), iBinValue(0);
								int iPos  = strData.Find(L',');
								if (-1 == iPos)
								{		 	
									VSI_FAIL(VSI_LOG_ERROR, L"Failed to get number of bins from data");
								}

								CString strNum = strData.Mid(0, iPos);
								iBinsNum = _wtoi(strNum);

								CComPtr<IVsiAnalHistogramInfo> pHistogramInfo;
								hr = pHistogramInfo.CoCreateInstance(__uuidof(VsiAnalHistogramInfo));
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

								hr = pHistogramInfo->Initialize(V_BSTR(&vName), L"", L"", L"", iBinsNum);
								VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

								// Get all of the samples that make up the data.
								int iStartPos = iPos + 1;
								for(int i = 0; i < iBinsNum - 1; ++i)
								{
									iPos  = strData.Find(L',', iStartPos);

									if (iPos > 0)
									{
										strNum = strData.Mid(iStartPos, iPos - iStartPos);
										iBinValue = _wtoi(strNum);
										pHistogramInfo->SetBinCount(i, iBinValue);
									}

									iStartPos = iPos + 1;
								}

								strNum = strData.Mid(iStartPos);
								iBinValue = _wtoi(strNum);
								pHistogramInfo->SetBinCount(iBinsNum - 1, iBinValue);

								{
									// Now dump the histogram samples and statistics.
									
									{
										ADD_SIMPLE_STRING_TO_STRING(L"Mean");
										ADD_VALUE_TO_STRING(vMean);
										ADD_NEWLINE_TO_STRING();
									}

									{
										ADD_SIMPLE_STRING_TO_STRING(L"Standard Deviation");			
										ADD_VALUE_TO_STRING(vStd);
										ADD_NEWLINE_TO_STRING();
									}

									ADD_SIMPLE_STRING_TO_STRING(L"Histogram Data");			
									ADD_NEWLINE_TO_STRING();

									CString strFormat;
									int iData = 0;
									for (int i=0;i<iBinsNum;++i)
									{
										strFormat.Format(L"%d", i);
										ADD_SIMPLE_STRING_TO_STRING(strFormat.GetString());
									
										hr = pHistogramInfo->GetBinCount(i, &iData);
										VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

										strFormat.Format(L"%d", iData);
										ADD_SIMPLE_STRING_TO_STRING(strFormat.GetString());
										ADD_NEWLINE_TO_STRING();
									}		
									ADD_SIMPLE_STRING_TO_STRING(L"End of data");
									ADD_NEWLINE_TO_STRING();
									ADD_NEWLINE_TO_STRING();
								}	
							}
						}
					}
				}
			}
		}

		// Convert and write output
		hr = WriteCSVFile(hFile, strOut); 
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failure writing to CSV file");
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::FillImageMap(IXMLDOMElement *pSeriesElement)
{
	HRESULT hr = S_OK;

	try
	{
		// Series path
		CComVariant vPath;
		hr = pSeriesElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_SERIES_PATH), &vPath);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		CString csSeriesPath(V_BSTR(&vPath));
		PathRemoveFileSpec((LPWSTR)(LPCWSTR)csSeriesPath);  // Remove series file
		// Find images
		CString strFilter;
		strFilter.Format(L"%s\\*." VSI_FILE_EXT_IMAGE, csSeriesPath);

		CString strImageFile;
		LPWSTR pszFile;
		CVsiFileIterator fileIter(strFilter);
		while (NULL != (pszFile = fileIter.Next()))
		{
			if (0 == _wcsicmp(CString(MAKEINTRESOURCE(IDS_SERIES_FILE_NAME)), pszFile))
				continue;
			if (0 == _wcsicmp(VSI_FILE_MEASUREMENT, pszFile))
				continue;

			strImageFile.Format(L"%s\\%s", csSeriesPath, pszFile);

			if (0 == _waccess(strImageFile, 0))
			{
				// Open file
				CComPtr<IXMLDOMDocument> pXmlDoc;
				hr = VsiCreateDOMDocument(&pXmlDoc);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Load configuration file failed");

				VARIANT_BOOL bLoaded = VARIANT_FALSE;
				hr = pXmlDoc->load(CComVariant(strImageFile), &bLoaded);
				if (hr != S_OK || bLoaded == VARIANT_FALSE)
					VSI_FAIL(VSI_LOG_ERROR, L"Load image file failed");

				// Find the root node in the XML document
				CComPtr<IXMLDOMElement> pXmlElemRoot;
				hr = pXmlDoc->get_documentElement(&pXmlElemRoot);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get root element");

				CComVariant vID;
				hr = pXmlElemRoot->getAttribute(CComBSTR(VSI_MSMNT_XML_MSMNT3D_ATT_ID), &vID);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read curve type");

				// Get image label
				CComVariant vName;
				hr = pXmlElemRoot->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read image name");

				CString strParam(L"settings/roots/root/groups/group/parameters/parameter[@name=\'Transducer-General/Center-Frequency\']/settings");

				CComPtr<IXMLDOMNode> pXmlFrequency;
				hr = pXmlElemRoot->selectSingleNode(CComBSTR(strParam), &pXmlFrequency);
				VSI_CHECK_S_OK(hr, VSI_LOG_ERROR, L"Failed to get settings element");

				CComQIPtr<IXMLDOMElement> pXmlFrequencyElement(pXmlFrequency);
				// Get image label
				CComVariant vFrequency;
				hr = pXmlFrequencyElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_VALUE), &vFrequency);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read image name");

				VSI_IMAGE_INFO imInfo;
				imInfo.m_strImageLabel = V_BSTR(&vName);
				imInfo.m_strFrequency = V_BSTR(&vFrequency);

				m_mapImages.insert(IMAGE_ID_NAME_MAP::value_type(CString(V_BSTR(&vID)), imInfo));
			}
		}
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::ExportPVLoopData(CString& strOut)
{
	HRESULT hr = S_OK;
	CString strTemp;

	try
	{
		// Write the samples for each loop in the set.
		UINT uiMaxLength = GetLongestLoop();

		// Spin through all of the data and output it. One sample per line.
		for (UINT uiIndex = 0; uiIndex < uiMaxLength; ++uiIndex)
		{
			strTemp.Format(L"%d", uiIndex + 1);	// Most users are 1 based.
			strOut += strTemp;

			CVsiVecMsmntPVInfoIter iter = m_vPVInfo.begin();
			for(; iter != m_vPVInfo.end(); ++iter)
			{
				CComPtr<IVsiAnalPressureVolumeInfo> pInfo = *iter;

				double dbTimeStamp, dbPressure, dbDerivative, dbDimension, dbVolume, dbECG;
				hr = pInfo->GetSample(uiIndex, &dbTimeStamp, &dbPressure, &dbDerivative, &dbDimension, &dbVolume, &dbECG);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

				if (S_OK == hr)
				{
					strTemp.Format(L"%s%f%s%f%s%f%s%f%s%f%s%f",
						m_szSptr, dbTimeStamp,
						m_szSptr, dbDimension,
						m_szSptr, dbVolume,
						m_szSptr, dbPressure,
						m_szSptr, dbDerivative,
						m_szSptr, dbECG);
					strOut += strTemp;
				}
				else
				{
					strTemp.Format(L"%s%s%s%s%s%s",
						m_szSptr,
						m_szSptr,
						m_szSptr,
						m_szSptr,
						m_szSptr,
						m_szSptr);
					strOut += strTemp;
				}
			}

			strOut += L"\r\n";
		}
	}
	VSI_CATCH(hr);

	return hr;

}

DWORD CVsiCSVWriter::GetLongestLoop()
{
	HRESULT hr = S_OK;
	DWORD dwLoopSize = 0;
	DWORD dwLongestLoop(0);


	CVsiVecMsmntPVInfoIter iter = m_vPVInfo.begin();
	for(; iter != m_vPVInfo.end(); ++iter)
	{
		CComPtr<IVsiAnalPressureVolumeInfo> pInfo = *iter;

		hr = pInfo->GetLength(&dwLoopSize);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		if (dwLoopSize > dwLongestLoop)
			dwLongestLoop = dwLoopSize;
	}

	return dwLongestLoop;
}

/// <summary>
///	Convert a protocol tag from the intermediate file to CSV
/// </summary>
///
/// <param name = "CString& strOut">Pointer to ouput string</param>
/// <param name = "IXMLDOMElement *pProtocolElement">Element containing the protocol information</param>
///
/// <returns>HRESULT : </returns>
HRESULT CVsiCSVWriter::ConvertProtocolMeasurements(CString& strOut, IXMLDOMElement *pCollectionElement, BOOL bGeneric)
{
	HRESULT hr = S_OK;
	CString strTemp;

	try
	{
		WCHAR szValue[80];
		WCHAR szSTD[80];
		long iMaxInstances(0);
		//int iHorn(0), iEmbryo(0);
		CComVariant vName, varModeName, varModeFlag, varEmbryo, varHorn, vValue;
		CComVariant vParamName, vParamUnits, vParamId, vParamPrecision;
		int iPrecisionParam = 6;

		if (!bGeneric)
		{
			CComVariant vMaxInstance;
			hr = pCollectionElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_MAX_INSTANCE), &vMaxInstance);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read maxInstance");
			hr = VariantChangeTypeEx(
				&vMaxInstance, &vMaxInstance,
				MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
				0, VT_UI4);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

			iMaxInstances = V_UI4(&vMaxInstance);
		}

		// Loop through all of the measurements under this protocol
		CComPtr<IXMLDOMNodeList> pMeasurementList;
		hr = pCollectionElement->selectNodes(CComBSTR(VSI_MSMNT_REPORT_XML_ELE_MSMNTS L"/" VSI_MSMNT_REPORT_XML_ELE_MSMNT), &pMeasurementList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get measurement list");

		long lNumMeasurements(0);
		hr = pMeasurementList->get_length(&lNumMeasurements);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get number of measurements");

		// If there are measurements under the protocol then go ahead and output the headings
		// for the measurement section.
		if (lNumMeasurements > 0)
		{
			CString strHeaderOne;

			strHeaderOne.Format(L"%s%s%s", m_szSptr, m_szSptr, m_szSptr);
			int iColumns = iMaxInstances - 1;
			if (iMaxInstances > 1 || 0 == iMaxInstances)
				iColumns += 2;

			CString strHeaderTwo;
			strHeaderTwo.Format(L"\"Measurement\"%s\"Mode\"%s\"Parameter\"%s\"Units\"", m_szSptr, m_szSptr, m_szSptr);

			// If there is more than one instance then we will output the average
			// and standard deviation.
			if (iMaxInstances > 1)
			{
				strTemp.Format(L"%s\"Avg\"%s\"STD\"", m_szSptr, m_szSptr);
				strHeaderTwo += strTemp;
			}

			if (iMaxInstances > 0)
			{
				for (int iInstance = 0; iInstance < iMaxInstances; ++iInstance)
				{
					strTemp.Format(L"%s\"Instance %d\"", m_szSptr, iInstance + 1);	// 1 Based for the user
					strHeaderTwo += strTemp;
				}
			}

			if (0 == iMaxInstances) // handle Generic package
			{
				strTemp.Format(L"%s\"Value\"", m_szSptr);
				strHeaderTwo += strTemp;
			}

			strOut += strHeaderOne;
			strOut += L"\r\n";
			strOut += strHeaderTwo;
			strOut += L"\r\n";
		}
		else
		{
			ReportNoMsmnts(strOut);
		}

		long lAscending = VsiGetDiscreteValue<long>(
			VSI_PDM_ROOT_APP, VSI_PDM_GROUP_GLOBAL,
			VSI_PARAMETER_UI_SORT_ORDER_ANALYSISBROWSER, m_pPdm);

		//create ordering map here. map labels to indexes to save memory. 
		//use indexes from ordered map to retrieve elements used for export
		report_order mo(lAscending);
		std::map<CString, int, report_order> mapMsmnts(mo);
		for (long lMeasurementIndex = 0; lMeasurementIndex < lNumMeasurements; ++lMeasurementIndex)
		{
			CComPtr<IXMLDOMNode> pMeasurementNode;
			hr = pMeasurementList->get_item(lMeasurementIndex, &pMeasurementNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get measurement node");

			CComQIPtr<IXMLDOMElement> pMeasurementElement(pMeasurementNode);
			VSI_CHECK_INTERFACE(pMeasurementElement, VSI_LOG_ERROR, L"Failed to get measurement element");

			// see if this is a dup msmnt that we should ignore
			vName.Clear();
			hr = pMeasurementElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_DUPLICATE), &vName);
			if (S_OK == hr && VT_NULL != V_VT(&vName))
			{
				vName.ChangeType(VT_BOOL);
				if (VARIANT_FALSE != V_BOOL(&vName))
					continue;
			}

			// Get the measurement name
			vName.Clear();
			hr = pMeasurementElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_LABEL), &vName);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read measurement name");

			CString strMsmntName;
			strMsmntName.Format(L"%s%d", V_BSTR(&vName), lMeasurementIndex);

			mapMsmnts.insert(std::map<CString, int>::value_type(strMsmntName, lMeasurementIndex));
		}
		//make the correct ordering
		std::map<CString, int, report_order>::iterator itr;
		for (itr = mapMsmnts.begin(); itr != mapMsmnts.end(); ++itr)
		{
			CComPtr<IXMLDOMNode> pMeasurementNode;
			hr = pMeasurementList->get_item((*itr).second, &pMeasurementNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get measurement node");

			CComQIPtr<IXMLDOMElement> pMeasurementElement(pMeasurementNode);
			VSI_CHECK_INTERFACE(pMeasurementElement, VSI_LOG_ERROR, L"Failed to get measurement element");

			// Get the measurement name
			vName.Clear();
			hr = pMeasurementElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_LABEL), &vName);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read measurement name");

			varModeName.Clear();
			hr = pMeasurementElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_MODE_NAME), &varModeName);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read mode name");

			varModeFlag.Clear();
			hr = pMeasurementElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_MODE_FLAG), &varModeFlag);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read mode name");

			// convert to display name

			DWORD dwFlag(VSI_MODE_FLAG_NONE);
			if (S_OK == hr && VT_NULL != V_VT(&varModeFlag))
			{
				hr = VariantChangeTypeEx(
					&varModeFlag, &varModeFlag,
					MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
					0, VT_UI4);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
				dwFlag = V_UI4(&varModeFlag);
			}

			CComVariant varDisplayName;
			hr = m_pModeMgr->GetModeNameFromInternalName(V_BSTR(&varModeName), dwFlag, &varDisplayName);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

			// Get the embryonic information
			CComVariant varEmbryo;
			hr = pMeasurementElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_EMBRYO_INDEX), &varEmbryo);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read embryo index");
			hr = VariantChangeTypeEx(
				&varEmbryo, &varEmbryo,
				MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
				0, VT_UI4);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
			int iEmbryo = V_UI4(&varEmbryo);

			// Insert parameter headers here
			hr = pMeasurementElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_EMBRYO_HORN), &varHorn);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read embryo index");
			hr = VariantChangeTypeEx(
				&varHorn, &varHorn,
				MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
				0, VT_I4);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
			int iHorn = V_I4(&varHorn);

			// Get all AVG elements from the measurement - this is the list of visible params
			CComPtr<IXMLDOMNodeList> pParameterList;
			hr = pMeasurementElement->selectNodes(CComBSTR(VSI_MSMNT_REPORT_XML_ELE_AVG), &pParameterList);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get parameter list");

			// Loop through all of the visible parameters for this measurement
			long lNumParameters(0);
			hr = pParameterList->get_length(&lNumParameters);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get number of parameters");

			for (long lParameterIndex = 0; lParameterIndex < lNumParameters; ++lParameterIndex)
			{
				CComPtr<IXMLDOMNode> pParameterNode;
				hr = pParameterList->get_item(lParameterIndex, &pParameterNode);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get parameter node");

				CComQIPtr<IXMLDOMElement> pParameterElement(pParameterNode);
				VSI_CHECK_INTERFACE(pParameterElement, VSI_LOG_ERROR, L"Failed to get parameter element");

				// Get the parameter name and units
				CComVariant vParamName;
				hr = pParameterElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vParamName);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read parameter name");

				// Get the parameter name and units
				CComVariant vParamAbbr;
				hr = pParameterElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_ABBREV), &vParamAbbr);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read parameter abbrev");

				// Get Param Id
				CComVariant vParamId;
				hr = pParameterElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PARAM_ID), &vParamId);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read parameter id");
				hr = VariantChangeTypeEx(
					&vParamId, &vParamId,
					MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
					0, VT_UI4);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

				CComVariant vParamUnits;
				hr = pParameterElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_UNITS), &vParamUnits);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read parameter units");

				WCHAR szTempName[80];
				if (m_bFemale && m_bPregnant && iHorn > 0)	// Add embryo information if relavant
				{
					swprintf_s(szTempName, _countof(szTempName), L"%s : %cE : %d",
						(LPCWSTR)FormatStringForCSV(V_BSTR(&vName)), (iHorn == 1) ? L'L' : L'R', iEmbryo);
				}
				else
				{
					wcscpy_s(szTempName, _countof(szTempName), (LPCWSTR)FormatStringForCSV(V_BSTR(&vName)));
				}

				strTemp.Format(L"%s%s%s%s%s%s%s%s",
					szTempName, m_szSptr,
					FormatStringForCSV(V_BSTR(&varDisplayName)).GetString(), m_szSptr,
					FormatStringForCSV(V_BSTR(&vParamName)).GetString(), m_szSptr,
					FormatStringForCSV(V_BSTR(&vParamUnits)).GetString(), m_szSptr);
				strOut += strTemp;

				// Get AVG value
				vValue.Clear();
				hr = pParameterElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_VALUE), &vValue);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read average value");

				hr = VariantChangeTypeEx(
					&vValue, &vValue,
					MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
					0, VT_R8);
				if (S_OK != hr)
				{
					hr = VariantChangeTypeEx(
						&vValue, &vValue,
						MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
						0, VT_BSTR);
					if (S_OK == hr)
					{
						if (lstrcmp(V_BSTR(&vValue), L"----") == 0)
						{
							wcscpy_s(szValue, V_BSTR(&vValue));
						}
						else
						{
							wcscpy_s(szValue, L"N/A");
						}
					}
					else
					{
						wcscpy_s(szValue, L"N/A");
					}
					hr = S_OK;
				}
				else
				{
					if (V_R8(&vValue) >= DBL_MAX || V_R8(&vValue) <= -DBL_MAX || _finite(V_R8(&vValue)) == 0)
					{
						wcscpy_s(szValue, L"N/A");
						hr = S_OK;
					}
					else
					{
						swprintf_s(szValue, L"%f", V_R8(&vValue));

						// Use user local for CSV output
						int iRet = VsiGetDoubleFormat(szValue, szValue, _countof(szValue), iPrecisionParam);
						VSI_WIN32_VERIFY(0 != iRet, VSI_LOG_ERROR, NULL);
					}
				}

				if (iMaxInstances > 1)
				{
					CComVariant vStdValue;
					hr = pParameterElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_STD), &vStdValue);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read STD value");

					wcscpy_s(szSTD, V_BSTR(&vStdValue));

					int iRet = VsiGetDoubleFormat(szSTD, szSTD, _countof(szSTD), iPrecisionParam);
					VSI_WIN32_VERIFY(0 != iRet, VSI_LOG_ERROR, NULL);

					strTemp.Format(L"%s%s%s%s", (LPCWSTR)FormatStringForCSV(szValue), m_szSptr, (LPCWSTR)FormatStringForCSV(szSTD), m_szSptr);
					strOut += strTemp;
				}

				// Output the individual instances if they are in the file.
				if (iMaxInstances > 0)
				{
					int iColumnsToSkip = iMaxInstances;

					// Find instance parameters with particular paramId
					CString strParam;
					strParam.Format(L"%i\']", V_UI4(&vParamId));
					CComBSTR bstrSearch(VSI_MSMNT_XML_ELE_INSTANCE L"/"
						VSI_MSMNT_XML_ELE_PARAMETERS L"/"
						VSI_MSMNT_XML_ELE_PARAMETER L"[@" 
						VSI_MSMNT_XML_ATT_PARAM_ID L"=\'");
					bstrSearch += strParam;

					CComPtr<IXMLDOMNodeList> pInstanceParamList;
					hr = pMeasurementElement->selectNodes(bstrSearch, &pInstanceParamList);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get instance parameter list");

					long lNumInstances(0);
					hr = pInstanceParamList->get_length(&lNumInstances);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get number of instances");

					for (long lInstanceIndex = 0; lInstanceIndex < lNumInstances; ++lInstanceIndex)
					{
						CComPtr<IXMLDOMNode> pInstanceParamNode;
						hr = pInstanceParamList->get_item(lInstanceIndex, &pInstanceParamNode);
						VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get parameter node");

						CComQIPtr<IXMLDOMElement> pInstanceParamElement(pInstanceParamNode);
						VSI_CHECK_INTERFACE(pInstanceParamElement, VSI_LOG_ERROR, L"Failed to get parameter element");

						vValue.Clear();
						hr = pInstanceParamElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_VALUE), &vValue);
						VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read instance parameter value");

						hr = VariantChangeTypeEx(
							&vValue, &vValue,
							MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
							0, VT_R8);
						if (S_OK != hr)
						{
							hr = VariantChangeTypeEx(
								&vValue, &vValue,
								MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
								0, VT_BSTR);
							if (S_OK == hr)
							{
								if (lstrcmp(V_BSTR(&vValue), L"----") == 0)
								{
									wcscpy_s(szValue, V_BSTR(&vValue));
								}
								else
								{
									wcscpy_s(szValue, L"N/A");
								}
							}
							else
							{
								wcscpy_s(szValue, L"N/A");
							}
							hr = S_OK;
						}
						else
						{
							if (V_R8(&vValue) >= DBL_MAX || V_R8(&vValue) <= -DBL_MAX || _finite(V_R8(&vValue)) == 0)
							{
								wcscpy_s(szValue, L"N/A");
								hr = S_OK;
							}
							else
							{
								swprintf_s(szValue, L"%f", V_R8(&vValue));

								// Use user local for CSV output
								int iRet = VsiGetDoubleFormat(szValue, szValue, _countof(szValue), iPrecisionParam);
								VSI_WIN32_VERIFY(0 != iRet, VSI_LOG_ERROR, NULL);
							}
						}

						strTemp.Format(L"%s", (LPCWSTR)FormatStringForCSV(szValue));
						strOut += strTemp;
						strOut += m_szSptr;

						--iColumnsToSkip;
					}

					// Complete missing blanks in the instance row
					strTemp.Empty();
					for (long lInstanceIndex = 0; lInstanceIndex < iColumnsToSkip; ++lInstanceIndex)
					{
						strTemp += m_szSptr;
					}

					strOut += strTemp;
				}

				if (0 == iMaxInstances) // handle Generic package
				{
					strTemp.Format(L"%s%s", (LPCWSTR)FormatStringForCSV(szValue), m_szSptr);
					strOut += strTemp;
				}
				strOut += L"\r\n";
			}
		}
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::ExportMsmntData(CString& strOut, IXMLDOMElement *pLoopElement)
{
	HRESULT hr = S_OK;
	try
	{
		// Find any associated measurement data sections
		CComPtr<IXMLDOMNodeList> pMsmntDataList;
		hr = pLoopElement->selectNodes(VSI_MSMNT_XML_ELE_MSMNT_DATA, &pMsmntDataList);
		if (S_OK == hr)//if we have data, dump them into csv
		{
			long lNumDataSegments(0);
			pMsmntDataList->get_length(&lNumDataSegments);

			if (0 == lNumDataSegments)
				return S_FALSE;

			for (long lDataSegmentIndex = 0; lDataSegmentIndex < lNumDataSegments; ++lDataSegmentIndex)
			{
				strOut += L"\r\n";
				CComPtr<IXMLDOMNode> pDataSegmentNode;
				hr = pMsmntDataList->get_item(lDataSegmentIndex, &pDataSegmentNode);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

				CComQIPtr<IXMLDOMElement> pDataSegmentElement(pDataSegmentNode);
				VSI_CHECK_INTERFACE(pDataSegmentElement, VSI_LOG_ERROR, L"Failed to get data segment element");

				CComVariant vType;
				hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_TYPE), &vType);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get type attribute");
				
				hr = VariantChangeTypeEx(
					&vType, &vType,
					MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
					0, VT_I4);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
				int nType = V_I4(&vType);

				switch(nType)
				{
				case VSI_EXPORT_DATA_TYPE_HISTOGRAM:
					hr = ExportHistogramData(strOut, pDataSegmentElement);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to export histogram data");
					break;
				case VSI_EXPORT_DATA_TYPE_CONTRAST:
					hr = ExportContrastData(strOut, pDataSegmentElement);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to export contrast data");
					break;
				case VSI_EXPORT_DATA_TYPE_CARDIAC:
					hr = ExportCardiacData(strOut, pDataSegmentElement);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to export cardiac data");
					break;
				case VSI_EXPORT_DATA_TYPE_PV:
					hr = ExportPVData(strOut, pDataSegmentElement);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to export PV data");
					break;
				case VSI_EXPORT_DATA_TYPE_BLV:
					hr = ExportBLVData(strOut, pDataSegmentElement);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to export BLV data");
					break;
				case VSI_EXPORT_DATA_TYPE_MLV:
					hr = ExportMLVData(strOut, pDataSegmentElement);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to export MLV data");
					break;
				case VSI_EXPORT_DATA_TYPE_PA:
					hr = ExportPAData(strOut, pDataSegmentElement);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to export MLV data");
					break;
				}
			}
		}
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::ExportHistogramData(CString& strOut, IXMLDOMElement *pDataSegmentElement)
{
	HRESULT hr = S_OK;
	try
	{
		CComVariant vName;
		hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get name attribute");
		strOut += FormatStringForCSV(V_BSTR(&vName));
		strOut += m_szSptr;
		CComVariant vModeName;
		hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ATT_MODE), &vModeName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get mode name attribute");
		strOut += FormatStringForCSV(V_BSTR(&vModeName));
		strOut += L"\r\n\r\n";

		CComPtr<IXMLDOMNode> pLoopNode;
		hr = pDataSegmentElement->get_parentNode(&pLoopNode);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get loop node");
		CComQIPtr<IXMLDOMElement> pLoopElement(pLoopNode);

		CComVariant vMean;
		hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_MEAN), &vMean);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vMean attribute");
		FormatVariantDouble(vMean);
		strOut += FormatStringForCSV(L"Mean");
		strOut += m_szSptr;
		strOut += FormatStringForCSV(V_BSTR(&vMean));
		strOut += L"\r\n";

		CComVariant vSTD;
		hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_STD), &vSTD);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vSTD attribute");
		FormatVariantDouble(vSTD);
		strOut += FormatStringForCSV(L"STD");
		strOut += m_szSptr;
		strOut += FormatStringForCSV(V_BSTR(&vSTD));
		strOut += L"\r\n";

		CComVariant vPctVas;
		hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PERCENT_VASCULARITY), &vPctVas);
		if (S_OK == hr)
		{
			FormatVariantDouble(vPctVas);
			strOut += FormatStringForCSV(L"Percent Vascularity");
			strOut += m_szSptr;
			strOut += FormatStringForCSV(V_BSTR(&vPctVas));
			strOut += L"\r\n";
		}

		CComVariant vHistoType;
		hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_HISTOGRAM_TYPE), &vHistoType);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vHistoType attribute");
		if (S_OK == hr)
		{
			hr = VariantChangeTypeEx(
				&vHistoType, &vHistoType,
				MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
				0, VT_I4);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
			DWORD dwType = V_I4(&vHistoType);
			if (dwType != 0)
			{
				LPCWSTR pszType(NULL);
				if (dwType == VSI_SYS_HISTOGRAM_RAW_DATA)
				{
					pszType = L"RAW Data";
				}
				else if (dwType == VSI_SYS_HISTOGRAM_IMAGE_DATA)
				{
					pszType = L"Image Data";
				}

				if (pszType != NULL)
				{
					strOut += FormatStringForCSV(L"Calculation");
					strOut += m_szSptr;					
					strOut += FormatStringForCSV(pszType);
					strOut += L"\r\n";
				}
			}
		}

		strOut += L"\r\n";

		CComPtr<IXMLDOMNodeList> pMsmntSampleList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ELE_SAMPLE, &pMsmntSampleList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data sample");

		long lNumSamples(0);
		pMsmntSampleList->get_length(&lNumSamples);

		for (long lSampleIndex = 0; lSampleIndex < lNumSamples; ++lSampleIndex)
		{
			CComPtr<IXMLDOMNode> pSampleNode;
			hr = pMsmntSampleList->get_item(lSampleIndex, &pSampleNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

			CComQIPtr<IXMLDOMElement> pSampleElement(pSampleNode);
			VSI_CHECK_INTERFACE(pSampleElement, VSI_LOG_ERROR, L"Failed to get data sample element");

			CComVariant vBin;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_BIN), &vBin);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get bin attribute");
			strOut += FormatStringForCSV(V_BSTR(&vBin));
			strOut += m_szSptr;

			CComVariant vVal;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_VALUE), &vVal);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get value attribute");
			strOut += FormatStringForCSV(V_BSTR(&vVal));
			strOut += L"\r\n";
		}

		strOut += L"\r\n";
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::ExportContrastData(CString& strOut, IXMLDOMElement *pDataSegmentElement)
{
	HRESULT hr = S_OK;
	try
	{
		CComVariant vName;
		hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get name attribute");
		strOut += FormatStringForCSV(V_BSTR(&vName));
		strOut += m_szSptr;

		CComPtr<IXMLDOMNode> pLoopNode;
		hr = pDataSegmentElement->get_parentNode(&pLoopNode);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get loop node");
		CComQIPtr<IXMLDOMElement> pLoopElement(pLoopNode);

		CComVariant vCurveName;
		hr = pLoopElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vCurveName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get name attribute");
		strOut += FormatStringForCSV(V_BSTR(&vCurveName));

		strOut += L"\r\n\r\n";

		CComPtr<IXMLDOMNodeList> pMsmntSampleList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ELE_SAMPLE, &pMsmntSampleList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data sample");

		long lNumSamples(0);
		pMsmntSampleList->get_length(&lNumSamples);

		// First row is header
		for (long lSampleIndex = 0; lSampleIndex < lNumSamples; ++lSampleIndex)
		{
			CComPtr<IXMLDOMNode> pSampleNode;
			hr = pMsmntSampleList->get_item(lSampleIndex, &pSampleNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

			CComQIPtr<IXMLDOMElement> pSampleElement(pSampleNode);
			VSI_CHECK_INTERFACE(pSampleElement, VSI_LOG_ERROR, L"Failed to get data sample element");

			CComVariant vBin;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_BIN), &vBin);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get bin attribute");
			strOut += FormatStringForCSV(V_BSTR(&vBin));
			strOut += m_szSptr;

			//CComVariant vAbsTime;
			//hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_ABSOLUTE_TIME), &vAbsTime);
			//VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get abs time attribute");
			//strOut += FormatStringForCSV(V_BSTR(&vAbsTime));
			//strOut += m_szSptr;

			CComVariant vRelTime;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_RELATIVE_TIME), &vRelTime);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get rel time attribute");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vRelTime);
			}
			strOut += FormatStringForCSV(V_BSTR(&vRelTime));
			strOut += m_szSptr;

			CComVariant vCAmp;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_CONTRAST_AMP), &vCAmp);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get contrast amplitude attribute");
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vCAmp);
				}
				strOut += FormatStringForCSV(V_BSTR(&vCAmp));
				strOut += m_szSptr;
			}

			CComVariant vCPow;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_CONTRAST_POW), &vCPow);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get contrast power attribute");
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vCPow);
				}
				strOut += FormatStringForCSV(V_BSTR(&vCPow));
				strOut += m_szSptr;
			}

			CComVariant vBAmp;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_B_AMP), &vBAmp);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get B-Mode amplitude attribute");
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vBAmp);
				}
				strOut += FormatStringForCSV(V_BSTR(&vBAmp));
				strOut += m_szSptr;
			}

			CComVariant vBPow;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_B_POW), &vBPow);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get B-Mode power attribute");
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vBPow);
				}
				strOut += FormatStringForCSV(V_BSTR(&vBPow));
				strOut += m_szSptr;
			}

			CComVariant vDest;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_DESTURCT), &vDest);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get b pow attribute");
			strOut += FormatStringForCSV(V_BSTR(&vDest));

			strOut += L"\r\n";
		}

		CComPtr<IXMLDOMNodeList> pMsmntFitList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_REPORT_XML_ELE_CURVE_FIT, &pMsmntFitList);
		if (S_OK == hr)
		{
			long lNumFits(0);
			pMsmntFitList->get_length(&lNumFits);
			if (0 < lNumFits)
			{
				strOut += L"\r\n";
				strOut += "\"Curve Fitting Data\"\r\n";
			}

			for (long lFitIndex = 0; lFitIndex < lNumFits; ++lFitIndex)
			{
				CComPtr<IXMLDOMNode> pFitNode;
				hr = pMsmntFitList->get_item(lFitIndex, &pFitNode);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

				CComQIPtr<IXMLDOMElement> pFitElement(pFitNode);
				VSI_CHECK_INTERFACE(pFitElement, VSI_LOG_ERROR, L"Failed to get data sample element");

				CComVariant vDB;
				hr = pFitElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_RATIO_DB), &vDB);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vDB attribute");
				if (lFitIndex > 0)
				{
					FormatVariantDouble(vDB);
				}
				strOut += FormatStringForCSV(V_BSTR(&vDB));
				strOut += m_szSptr;

				CComVariant vLin;
				hr = pFitElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_RATIO_LIN), &vLin);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vLin attribute");
				if (lFitIndex > 0)
				{
					FormatVariantDouble(vLin);
				}
				strOut += FormatStringForCSV(V_BSTR(&vLin));
				strOut += m_szSptr;

				CComVariant vCh;
				hr = pFitElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_CHANGE_INTENSITY), &vCh);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vCh attribute");
				if (lFitIndex > 0)
				{
					FormatVariantDouble(vCh);
				}
				strOut += FormatStringForCSV(V_BSTR(&vCh));
				strOut += m_szSptr;

				CComVariant vA;
				hr = pFitElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PARAM_A), &vA);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vA attribute");
				if (lFitIndex > 0)
				{
					FormatVariantDouble(vA);
				}
				strOut += FormatStringForCSV(V_BSTR(&vA));
				strOut += m_szSptr;

				CComVariant vB;
				hr = pFitElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PARAM_B), &vB);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vB attribute");
				if (lFitIndex > 0)
				{
					FormatVariantDouble(vB);
				}
				strOut += FormatStringForCSV(V_BSTR(&vB));
				strOut += m_szSptr;

				CComVariant vSTD;
				hr = pFitElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_STD), &vSTD);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vSTD attribute");
				if (lFitIndex > 0)
				{
					FormatVariantDouble(vSTD);
				}
				strOut += FormatStringForCSV(V_BSTR(&vSTD));
				strOut += L"\r\n";
			}
			strOut += L"\r\n";
		}
		//export microbubble analysis
		CComPtr<IXMLDOMNodeList> pPerfusionList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ATT_PERFUSION, &pPerfusionList);
		if (S_OK == hr)
		{
			long lNumPerfusion(0);
			pPerfusionList->get_length(&lNumPerfusion);
			if (0 < lNumPerfusion)
			{
				strOut += L"\r\n";
				strOut += "\"Perfusion Analysis Data\"\r\n";
			}

			for (long lPerfusionIndex = 0; lPerfusionIndex < lNumPerfusion; ++lPerfusionIndex)
			{
				CComPtr<IXMLDOMNode> pPerfusionNode;
				hr = pPerfusionList->get_item(lPerfusionIndex, &pPerfusionNode);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

				CComQIPtr<IXMLDOMElement> pPerfusionElement(pPerfusionNode);
				VSI_CHECK_INTERFACE(pPerfusionElement, VSI_LOG_ERROR, L"Failed to get data sample element");

				CComVariant vLin;
				hr = pPerfusionElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PEAK_ENHANCEMENT_L), &vLin);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vLin attribute");
				if (lPerfusionIndex > 0)
				{
					FormatVariantDouble(vLin);
				}
				strOut += FormatStringForCSV(V_BSTR(&vLin));
				strOut += m_szSptr;

				CComVariant vDB;
				hr = pPerfusionElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PEAK_ENHANCEMENT_DB), &vDB);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vDB attribute");
				if (lPerfusionIndex > 0)
				{
					FormatVariantDouble(vDB);
				}
				strOut += FormatStringForCSV(V_BSTR(&vDB));
				strOut += m_szSptr;

				CComVariant vTimeToPeak;
				hr = pPerfusionElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_TIME_TO_PEAK), &vTimeToPeak);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vTimeToPeak attribute");
				if (lPerfusionIndex > 0)
				{
					FormatVariantDouble(vTimeToPeak);
				}
				strOut += FormatStringForCSV(V_BSTR(&vTimeToPeak));
				strOut += L"\r\n";
			}
			strOut += L"\r\n";
		}
		//export microbubble analysis
		CComPtr<IXMLDOMNodeList> pTargetedList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ATT_TARGETED, &pTargetedList);
		if (S_OK == hr)
		{
			long lNumTargeted(0);
			pTargetedList->get_length(&lNumTargeted);
			if (0 < lNumTargeted)
			{
				strOut += L"\r\n";
				strOut += "\"Targeted Analysis Data\"\r\n";
			}

			for (long lTargetedIndex = 0; lTargetedIndex < lNumTargeted; ++lTargetedIndex)
			{
				CComPtr<IXMLDOMNode> pTargetedNode;
				hr = pTargetedList->get_item(lTargetedIndex, &pTargetedNode);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

				CComQIPtr<IXMLDOMElement> pTargetedElement(pTargetedNode);
				VSI_CHECK_INTERFACE(pTargetedElement, VSI_LOG_ERROR, L"Failed to get data sample element");

				CComVariant vLin;
				hr = pTargetedElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_TARGET_ENHANCEMENT_L), &vLin);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vLin attribute");
				if (lTargetedIndex > 0)
				{
					FormatVariantDouble(vLin);
				}
				strOut += FormatStringForCSV(V_BSTR(&vLin));
				strOut += m_szSptr;

				CComVariant vDB;
				hr = pTargetedElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_TARGET_ENHANCEMENT_DB), &vDB);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vDB attribute");
				if (lTargetedIndex > 0)
				{
					FormatVariantDouble(vDB);
				}
				strOut += FormatStringForCSV(V_BSTR(&vDB));
				strOut += m_szSptr;

				CComVariant vDiff;
				hr = pTargetedElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_TARGET_ENHANCEMENT_DIFF), &vDiff);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vDiff attribute");
				if (lTargetedIndex > 0)
				{
					FormatVariantDouble(vDiff);
				}
				strOut += FormatStringForCSV(V_BSTR(&vDiff));
				strOut += L"\r\n";
			}
			strOut += L"\r\n";
		}
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::ExportCardiacData(CString& strOut, IXMLDOMElement *pDataSegmentElement)
{
	HRESULT hr = S_OK;
	try
	{
		CComVariant vName;
		hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get name attribute");
		strOut += FormatStringForCSV(V_BSTR(&vName));
		strOut += L"\r\n\r\n";

		CComPtr<IXMLDOMNodeList> pMsmntSampleList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ELE_SAMPLE, &pMsmntSampleList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data sample");

		long lNumSamples(0);
		pMsmntSampleList->get_length(&lNumSamples);

		// First row is header
		for (long lSampleIndex = 0; lSampleIndex < lNumSamples; ++lSampleIndex)
		{
			CComPtr<IXMLDOMNode> pSampleNode;
			hr = pMsmntSampleList->get_item(lSampleIndex, &pSampleNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

			CComQIPtr<IXMLDOMElement> pSampleElement(pSampleNode);
			VSI_CHECK_INTERFACE(pSampleElement, VSI_LOG_ERROR, L"Failed to get data sample element");

			CComVariant vBin;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_BIN), &vBin);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get bin attribute");

			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vBin);
			}

			strOut += FormatStringForCSV(V_BSTR(&vBin));
			strOut += m_szSptr;

			CComPtr<IXMLDOMNodeList> pMsmntFrameList;
			hr = pSampleElement->selectNodes(VSI_MSMNT_XML_ELE_FRAME, &pMsmntFrameList);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data sample");

			long lNumFrames(0);
			pMsmntFrameList->get_length(&lNumFrames);

			for (long lFrameIndex = 0; lFrameIndex < lNumFrames; ++lFrameIndex)
			{
				CComPtr<IXMLDOMNode> pFrameNode;
				hr = pMsmntFrameList->get_item(lFrameIndex, &pFrameNode);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

				CComQIPtr<IXMLDOMElement> pFrameElement(pFrameNode);
				VSI_CHECK_INTERFACE(pFrameElement, VSI_LOG_ERROR, L"Failed to get frame element");

				CComVariant vCAmp;
				hr = pFrameElement->getAttribute(VSI_MSMNT_XML_ATT_CONTRAST_AMP, &vCAmp);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read camp");
				if (S_OK == hr)
				{
					if (lSampleIndex > 0)
					{
						FormatVariantDouble(vCAmp);
					}
					strOut += FormatStringForCSV(V_BSTR(&vCAmp));
					strOut += m_szSptr;
				}

				CComVariant vCPow;
				hr = pFrameElement->getAttribute(VSI_MSMNT_XML_ATT_CONTRAST_POW, &vCPow);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read cpow");
				if (S_OK == hr)
				{
					if (lSampleIndex > 0)
					{
						FormatVariantDouble(vCPow);
					}
					strOut += FormatStringForCSV(V_BSTR(&vCPow));
					strOut += m_szSptr;
				}

				CComVariant vBAmp;
				hr = pFrameElement->getAttribute(VSI_MSMNT_XML_ATT_B_AMP, &vBAmp);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read bamp");
				if (S_OK == hr)
				{
					if (lSampleIndex > 0)
					{
						FormatVariantDouble(vBAmp);
					}
					strOut += FormatStringForCSV(V_BSTR(&vBAmp));
					strOut += m_szSptr;
				}

				CComVariant vBPow;
				hr = pFrameElement->getAttribute(VSI_MSMNT_XML_ATT_B_POW, &vBPow);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read bpow");
				if (S_OK == hr)
				{
					if (lSampleIndex > 0)
					{
						FormatVariantDouble(vBPow);
					}
					strOut += FormatStringForCSV(V_BSTR(&vBPow));
					strOut += m_szSptr;
				}
			}

			CComVariant vCAmpAvg;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_CONTRAST_AMP_AVG, &vCAmpAvg);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read campavg");
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vCAmpAvg);
				}
				strOut += FormatStringForCSV(V_BSTR(&vCAmpAvg));
				strOut += m_szSptr;
			}

			CComVariant vCPowAvg;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_CONTRAST_POW_AVG, &vCPowAvg);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read abs cpowavg");
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vCPowAvg);
				}
				strOut += FormatStringForCSV(V_BSTR(&vCPowAvg));
				strOut += m_szSptr;
			}
			
			CComVariant vBAmpAvg;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_B_AMP_AVG, &vBAmpAvg);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read bampavg");
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vBAmpAvg);
				}
				strOut += FormatStringForCSV(V_BSTR(&vBAmpAvg));
				strOut += m_szSptr;
			}

			CComVariant vBPowAvg;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_B_POW_AVG, &vBPowAvg);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read bpowavg");
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vBPowAvg);
				}
				strOut += FormatStringForCSV(V_BSTR(&vBPowAvg));
			}
			
			strOut += L"\r\n";
		}
		strOut += L"\r\n";
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::ExportPVData(CString& strOut, IXMLDOMElement *pDataSegmentElement)
{
	HRESULT hr = S_OK;
	try
	{
		CComVariant vName;
		hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get name attribute");
		strOut += FormatStringForCSV(V_BSTR(&vName));
		strOut += m_szSptr;
		CComVariant vMsmntName;
		hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_REPORT_XML_ELE_MSMNT), &vMsmntName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get measurement attribute");
		strOut += FormatStringForCSV(V_BSTR(&vMsmntName));
		strOut += L"\r\n\r\n";

		CComPtr<IXMLDOMNodeList> pMsmntSampleList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ELE_SAMPLE, &pMsmntSampleList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data sample");

		long lNumSamples(0);
		pMsmntSampleList->get_length(&lNumSamples);

		// First row is the header
		for (long lSampleIndex = 0; lSampleIndex < lNumSamples; ++lSampleIndex)
		{
			CComPtr<IXMLDOMNode> pSampleNode;
			hr = pMsmntSampleList->get_item(lSampleIndex, &pSampleNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

			CComQIPtr<IXMLDOMElement> pSampleElement(pSampleNode);
			VSI_CHECK_INTERFACE(pSampleElement, VSI_LOG_ERROR, L"Failed to get data sample element");

			CComVariant vBin;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_BIN), &vBin);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get bin attribute");
			strOut += FormatStringForCSV(V_BSTR(&vBin));
			strOut += m_szSptr;
			CComVariant vAbsTime;

			// Bug 15472 - Switched to VSI_MSMNT_XML_ATT_RELATIVE_TIME
			//hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_ABSOLUTE_TIME, &vAbsTime);
			//VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read abs time");

			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_RELATIVE_TIME, &vAbsTime);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read rel time");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vAbsTime);
			}
			strOut += FormatStringForCSV(V_BSTR(&vAbsTime));
			strOut += m_szSptr;
			CComVariant vDim;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_DIMENSION, &vDim);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vDim");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vDim);
			}
			strOut += FormatStringForCSV(V_BSTR(&vDim));
			strOut += m_szSptr;
			CComVariant vVol;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_VOLUME, &vVol);
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vVol);
				}
				strOut += FormatStringForCSV(V_BSTR(&vVol));
				strOut += m_szSptr;
			}
			CComVariant vPress;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_PRESSURE, &vPress);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vPress");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vPress);
			}
			strOut += FormatStringForCSV(V_BSTR(&vPress));
			strOut += m_szSptr;
			CComVariant vDer;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_DERIVATIVE, &vDer);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vDer");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vDer);
			}
			strOut += FormatStringForCSV(V_BSTR(&vDer));
			strOut += m_szSptr;
			CComVariant vAmp;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_ECG_AMPLITUDE, &vAmp);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vAmp");
			strOut += FormatStringForCSV(V_BSTR(&vAmp));
			strOut += m_szSptr;
			CComVariant vVolAvg;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_VOLUME_AVG, &vVolAvg);
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vVolAvg);
				}
				strOut += FormatStringForCSV(V_BSTR(&vVolAvg));
				strOut += m_szSptr;
			}
			CComVariant vPressAvg;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_PRESSURE_AVG, &vPressAvg);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vPressAvg");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vPressAvg);
			}
			strOut += FormatStringForCSV(V_BSTR(&vPressAvg));
			strOut += m_szSptr;
			CComVariant vDerAvg;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_DERIVATIVE_AVG, &vDerAvg);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vDerAvg");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vDerAvg);
			}
			strOut += FormatStringForCSV(V_BSTR(&vDerAvg));
			strOut += m_szSptr;
			CComVariant vAmpAvg;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_ECG_AMPLITUDE_AVG, &vAmpAvg);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vAmpAvg");
			strOut += FormatStringForCSV(V_BSTR(&vAmpAvg));
			strOut += L"\r\n";
		}

		CComPtr<IXMLDOMNodeList> pMsmntSampleESPVRList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ELE_ESPVR, &pMsmntSampleESPVRList);
		if (S_OK == hr)
		{
			strOut += FormatStringForCSV(L"ESPVR");
			strOut += L"\r\n";

			long lNumSamples(0);
			pMsmntSampleESPVRList->get_length(&lNumSamples);

			for (long lSampleIndex = 0; lSampleIndex < lNumSamples; ++lSampleIndex)
			{
				CComPtr<IXMLDOMNode> pSampleNode;
				hr = pMsmntSampleESPVRList->get_item(lSampleIndex, &pSampleNode);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

				CComQIPtr<IXMLDOMElement> pSampleElement(pSampleNode);
				VSI_CHECK_INTERFACE(pSampleElement, VSI_LOG_ERROR, L"Failed to get data sample element");

				CComVariant vSlope;
				hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_SLOPE), &vSlope);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vSlope attribute");
				if (NULL != wcschr(V_BSTR(&vSlope), L'#'))
				{
					strOut += FormatStringForCSV(L"N/A");
				}
				else
				{
					if (lSampleIndex > 0)
					{
						FormatVariantDouble(vSlope);
					}
					strOut += FormatStringForCSV(V_BSTR(&vSlope));
				}
				strOut += m_szSptr;

				CComVariant vIntercept;
				hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_INTERCEPT), &vIntercept);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vIntercept attribute");
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vIntercept);
				}
				strOut += FormatStringForCSV(V_BSTR(&vIntercept));
				strOut += L"\r\n";
			}

			CComPtr<IXMLDOMNode> pMsmntSysTimesNode;
			hr = pDataSegmentElement->selectSingleNode(VSI_MSMNT_XML_ELE_SYS_TIMES, &pMsmntSysTimesNode);
			if (S_OK == hr)
			{
				CComQIPtr<IXMLDOMElement> pMsmntSysTimes(pMsmntSysTimesNode);
				VSI_CHECK_INTERFACE(pMsmntSysTimes, VSI_LOG_ERROR, L"Failed to get data sample element");

				CComVariant vName;
				hr = pMsmntSysTimes->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vName attribute");
				strOut += FormatStringForCSV(V_BSTR(&vName));

				CComPtr<IXMLDOMNodeList> pMsmntTimesList;
				hr = pMsmntSysTimes->selectNodes(VSI_MSMNT_XML_ELE_SAMPLE, &pMsmntTimesList);

				long lNumTimes(0);
				pMsmntTimesList->get_length(&lNumTimes);

				for (long lTimeIndex = 0; lTimeIndex < lNumTimes; ++lTimeIndex)
				{
					strOut += m_szSptr;
					CComPtr<IXMLDOMNode> pTimeNode;
					hr = pMsmntTimesList->get_item(lTimeIndex, &pTimeNode);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

					CComQIPtr<IXMLDOMElement> pTimeElement(pTimeNode);
					VSI_CHECK_INTERFACE(pTimeElement, VSI_LOG_ERROR, L"Failed to get data sample element");

					CComVariant vTime;
					hr = pTimeElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_TIME), &vTime);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vTime attribute");
					FormatVariantDouble(vTime);
					strOut += FormatStringForCSV(V_BSTR(&vTime));
				}
			}
		}

		strOut += L"\r\n\r\n";

		CComPtr<IXMLDOMNodeList> pMsmntSampleEDPVRList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ELE_EDPVR, &pMsmntSampleEDPVRList);
		if (S_OK == hr)
		{
			strOut += FormatStringForCSV(L"EDPVR");
			strOut += L"\r\n";

			long lNumSamples(0);
			pMsmntSampleEDPVRList->get_length(&lNumSamples);

			for (long lSampleIndex = 0; lSampleIndex < lNumSamples; ++lSampleIndex)
			{
				CComPtr<IXMLDOMNode> pSampleNode;
				hr = pMsmntSampleEDPVRList->get_item(lSampleIndex, &pSampleNode);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

				CComQIPtr<IXMLDOMElement> pSampleElement(pSampleNode);
				VSI_CHECK_INTERFACE(pSampleElement, VSI_LOG_ERROR, L"Failed to get data sample element");

				CComVariant vSlope;
				hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_SLOPE), &vSlope);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vSlope attribute");
				if (NULL != wcschr(V_BSTR(&vSlope), L'#'))
				{
					strOut += FormatStringForCSV(L"N/A");
				}
				else
				{
					if (lSampleIndex > 0)
					{
						FormatVariantDouble(vSlope);
					}
					strOut += FormatStringForCSV(V_BSTR(&vSlope));
				}
				strOut += m_szSptr;

				CComVariant vIntercept;
				hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_INTERCEPT), &vIntercept);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vIntercept attribute");
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vIntercept);
				}
				strOut += FormatStringForCSV(V_BSTR(&vIntercept));
				strOut += L"\r\n";
			}

			CComPtr<IXMLDOMNode> pMsmntDiaTimesNode;
			hr = pDataSegmentElement->selectSingleNode(VSI_MSMNT_XML_ELE_DIA_TIMES, &pMsmntDiaTimesNode);
			if (S_OK == hr)
			{
				CComQIPtr<IXMLDOMElement> pMsmntDiaTimes(pMsmntDiaTimesNode);
				VSI_CHECK_INTERFACE(pMsmntDiaTimes, VSI_LOG_ERROR, L"Failed to get data sample element");

				CComVariant vName;
				hr = pMsmntDiaTimes->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vName attribute");
				strOut += FormatStringForCSV(V_BSTR(&vName));

				CComPtr<IXMLDOMNodeList> pMsmntTimesList;
				hr = pMsmntDiaTimes->selectNodes(VSI_MSMNT_XML_ELE_SAMPLE, &pMsmntTimesList);

				long lNumTimes(0);
				pMsmntTimesList->get_length(&lNumTimes);

				for (long lTimeIndex = 0; lTimeIndex < lNumTimes; ++lTimeIndex)
				{
					strOut += m_szSptr;
					CComPtr<IXMLDOMNode> pTimeNode;
					hr = pMsmntTimesList->get_item(lTimeIndex, &pTimeNode);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

					CComQIPtr<IXMLDOMElement> pTimeElement(pTimeNode);
					VSI_CHECK_INTERFACE(pTimeElement, VSI_LOG_ERROR, L"Failed to get data sample element");

					CComVariant vTime;
					hr = pTimeElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_TIME), &vTime);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get vTime attribute");
					FormatVariantDouble(vTime);
					strOut += FormatStringForCSV(V_BSTR(&vTime));
				}
			}		
		}
		strOut += L"\r\n\r\n";
		hr = S_OK;
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::ExportBLVData(CString& strOut, IXMLDOMElement *pDataSegmentElement)
{
	HRESULT hr = S_OK;
	try
	{
		CComVariant vName;
		hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get name attribute");
		strOut += V_BSTR(&vName);
		strOut += L"\r\n\r\n";

		CComPtr<IXMLDOMNodeList> pMsmntSampleList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ELE_SAMPLE, &pMsmntSampleList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data sample");

		long lNumSamples(0);
		pMsmntSampleList->get_length(&lNumSamples);

		// 1st row is the header
		for (long lSampleIndex = 0; lSampleIndex < lNumSamples; ++lSampleIndex)
		{
			CComPtr<IXMLDOMNode> pSampleNode;
			hr = pMsmntSampleList->get_item(lSampleIndex, &pSampleNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

			CComQIPtr<IXMLDOMElement> pSampleElement(pSampleNode);
			VSI_CHECK_INTERFACE(pSampleElement, VSI_LOG_ERROR, L"Failed to get data sample element");

			CComVariant vBin;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_BIN), &vBin);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get bin attribute");
			strOut += FormatStringForCSV(V_BSTR(&vBin));
			strOut += m_szSptr;

			CComVariant vAbsTime;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_RELATIVE_TIME, &vAbsTime);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read abs time");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vAbsTime);
			}

			strOut += FormatStringForCSV(V_BSTR(&vAbsTime));
			strOut += m_szSptr;

			CComVariant vArea;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_AREA, &vArea);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vArea");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vArea);
			}

			strOut += FormatStringForCSV(V_BSTR(&vArea));
			strOut += m_szSptr;
			CComVariant vVol;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_VOLUME, &vVol);
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vVol);
				}
				strOut += FormatStringForCSV(V_BSTR(&vVol));
				strOut += m_szSptr;
			}
			CComVariant vPress;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_PRESSURE, &vPress);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vPress");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vPress);
			}
			strOut += FormatStringForCSV(V_BSTR(&vPress));
			strOut += m_szSptr;
			CComVariant vAmp;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_ECG_AMPLITUDE, &vAmp);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vAmp");
			strOut += FormatStringForCSV(V_BSTR(&vAmp));
			strOut += L"\r\n";
		}
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::ExportPAData(CString& strOut, IXMLDOMElement *pDataSegmentElement)
{
	HRESULT hr = S_OK;
	try
	{
		CComVariant vName;
		hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get name attribute");
		strOut += V_BSTR(&vName);
		strOut += L"\r\n\r\n";

		CComPtr<IXMLDOMNodeList> pMsmntSampleList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ELE_SAMPLE, &pMsmntSampleList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data sample");

		long lNumSamples(0);
		pMsmntSampleList->get_length(&lNumSamples);

		// First row is the header
		for (long lSampleIndex = 0; lSampleIndex < lNumSamples; ++lSampleIndex)
		{
			CComPtr<IXMLDOMNode> pSampleNode;
			hr = pMsmntSampleList->get_item(lSampleIndex, &pSampleNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

			CComQIPtr<IXMLDOMElement> pSampleElement(pSampleNode);
			VSI_CHECK_INTERFACE(pSampleElement, VSI_LOG_ERROR, L"Failed to get data sample element");

			CComVariant vBin;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_BIN), &vBin);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get bin attribute");
			strOut += FormatStringForCSV(V_BSTR(&vBin));
			strOut += m_szSptr;

			CComVariant vAbsTime;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_RELATIVE_TIME, &vAbsTime);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read abs time");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vAbsTime);
			}
			strOut += FormatStringForCSV(V_BSTR(&vAbsTime));
			strOut += m_szSptr;

			CComVariant vPAValue;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_PA_VALUE, &vPAValue);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vPAValue");
			if (hr == S_OK)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vPAValue);
				}
				strOut += FormatStringForCSV(V_BSTR(&vPAValue));
				strOut += m_szSptr;
			}

			CComVariant vWLValue;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_WAVELENGTH, &vWLValue);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vWLValue");
			if (hr == S_OK)
			{
				strOut += FormatStringForCSV(V_BSTR(&vWLValue));
				strOut += m_szSptr;
			}

			CComVariant vSo2Value;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_SO2_VALUE, &vSo2Value);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vSo2Value");
			if (hr == S_OK)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vSo2Value);
				}
				strOut += FormatStringForCSV(V_BSTR(&vSo2Value));
				strOut += m_szSptr;
			}

			CComVariant vSo2ValueTotal;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_SO2_TOTAL_VALUE, &vSo2ValueTotal);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vSo2ValueTotal");
			if (hr == S_OK)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vSo2ValueTotal);
				}
				strOut += FormatStringForCSV(V_BSTR(&vSo2ValueTotal));
				strOut += m_szSptr;
			}

			CComVariant vHbtValue;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_HBT_VALUE, &vHbtValue);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vHbtValue");
			if (hr == S_OK)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vHbtValue);
				}
				strOut += FormatStringForCSV(V_BSTR(&vHbtValue));
				strOut += m_szSptr;
			}

			strOut += L"\r\n";			
		}
		strOut += L"\r\n";			
	}
	VSI_CATCH(hr);

	return hr;
}

HRESULT CVsiCSVWriter::ExportMLVData(CString& strOut, IXMLDOMElement *pDataSegmentElement)
{
	HRESULT hr = S_OK;
	try
	{
		CComVariant vName;
		hr = pDataSegmentElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_NAME), &vName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get name attribute");
		strOut += V_BSTR(&vName);
		strOut += L"\r\n\r\n";

		CComPtr<IXMLDOMNodeList> pMsmntSampleList;
		hr = pDataSegmentElement->selectNodes(VSI_MSMNT_XML_ELE_SAMPLE, &pMsmntSampleList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data sample");

		long lNumSamples(0);
		pMsmntSampleList->get_length(&lNumSamples);

		// First row is the header
		for (long lSampleIndex = 0; lSampleIndex < lNumSamples; ++lSampleIndex)
		{
			CComPtr<IXMLDOMNode> pSampleNode;
			hr = pMsmntSampleList->get_item(lSampleIndex, &pSampleNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get data segment node");

			CComQIPtr<IXMLDOMElement> pSampleElement(pSampleNode);
			VSI_CHECK_INTERFACE(pSampleElement, VSI_LOG_ERROR, L"Failed to get data sample element");

			CComVariant vBin;
			hr = pSampleElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_BIN), &vBin);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get bin attribute");
			strOut += FormatStringForCSV(V_BSTR(&vBin));
			strOut += m_szSptr;
			CComVariant vAbsTime;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_RELATIVE_TIME, &vAbsTime);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read abs time");
			strOut += FormatStringForCSV(V_BSTR(&vAbsTime));
			strOut += m_szSptr;
			CComVariant vWallIn;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_WALL_INNER, &vWallIn);
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vWallIn);
				}
				strOut += FormatStringForCSV(V_BSTR(&vWallIn));
				strOut += m_szSptr;
			}
			CComVariant vDiam;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_DIAMETER, &vDiam);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vDiam");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vDiam);
			}
			strOut += FormatStringForCSV(V_BSTR(&vDiam));
			strOut += m_szSptr;
			CComVariant vWallOut;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_WALL_OUTER, &vWallOut);
			if (S_OK == hr)
			{
				if (lSampleIndex > 0)
				{
					FormatVariantDouble(vWallOut);
				}
				strOut += FormatStringForCSV(V_BSTR(&vWallOut));
				strOut += m_szSptr;
			}
			CComVariant vPress;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_PRESSURE, &vPress);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vPress");
			if (lSampleIndex > 0)
			{
				FormatVariantDouble(vPress);
			}
			strOut += FormatStringForCSV(V_BSTR(&vPress));
			strOut += m_szSptr;
			CComVariant vAmp;
			hr = pSampleElement->getAttribute(VSI_MSMNT_XML_ATT_ECG_AMPLITUDE, &vAmp);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read vAmp");
			strOut += FormatStringForCSV(V_BSTR(&vAmp));
			strOut += L"\r\n";
		}
	}
	VSI_CATCH(hr);

	return hr;
}

/// <summary>
///	Convert the list of calculations in the intermediate file to CSV.
/// </summary>
///
/// <param name = "CString& strOut">Pointer to output string</param>
/// <param name = "IXMLDOMElement *pProtocolElement"></param>
///
/// <returns>HRESULT : </returns>
HRESULT CVsiCSVWriter::ConvertProtocolCalculations(CString& strOut, IXMLDOMElement *pProtocolElement)
{
	HRESULT hr = S_OK;

	CComVariant vName, vUnits, vEmbryoHorn, vEmbryoIndex, vValue;
	int iPrecision = 6;

	try
	{
		CString strTemp;
		WCHAR szValue[80];

		// Loop through all of the calculations under this protocol
		CComPtr<IXMLDOMNodeList> pCalculationList;
		hr = pProtocolElement->selectNodes(CComBSTR(VSI_MSMNT_XML_ELE_CALCULATIONS L"/"
			VSI_MSMNT_XML_ELE_CALCULATION), &pCalculationList);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get calculation list");

		long lNumCalculations(0);
		hr = pCalculationList->get_length(&lNumCalculations);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get number of calculations");

		// If there are calculations listed under the protocol then go ahead and output
		// the calculation header stings.
		if (lNumCalculations > 0)
		{
			strOut += L"\r\n";

			strTemp.Format(L"Calculation%s%sUnits%s", m_szSptr, m_szSptr, m_szSptr);
			strOut += strTemp;

			strOut += L"\r\n";
		}

		long lAscending = VsiGetDiscreteValue<long>(
			VSI_PDM_ROOT_APP, VSI_PDM_GROUP_GLOBAL,
			VSI_PARAMETER_UI_SORT_ORDER_ANALYSISBROWSER, m_pPdm);

		//create ordering map here. map labels to indexes to save memory. 
		//use indexes from ordered map to retrieve elements used for export
		report_order mo(lAscending);
		std::map<CString, int, report_order> mapCalculations(mo);
		for (long lCalculationIndex = 0; lCalculationIndex < lNumCalculations; ++lCalculationIndex)
		{
			CComPtr<IXMLDOMNode> pCalculationNode;
			hr = pCalculationList->get_item(lCalculationIndex, &pCalculationNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get loop node");

			CComQIPtr<IXMLDOMElement> pCalculationElement(pCalculationNode);
			VSI_CHECK_INTERFACE(pCalculationElement, VSI_LOG_ERROR, L"Failed to get loop element");

			// Get the measurement name
			vName.Clear();
			hr = pCalculationElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_CALC_NAME), &vName);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read measurement name");

			mapCalculations.insert(std::map<CString, int>::value_type(CString(V_BSTR(&vName)), lCalculationIndex));
		}

		std::map<CString, int, report_order>::iterator itr;
		for (itr = mapCalculations.begin(); itr != mapCalculations.end(); ++itr)
		{
			CComPtr<IXMLDOMNode> pCalculationNode;
			hr = pCalculationList->get_item((*itr).second, &pCalculationNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get calculation node");

			CComQIPtr<IXMLDOMElement> pCalculationElement(pCalculationNode);
			VSI_CHECK_INTERFACE(pCalculationElement, VSI_LOG_ERROR, L"Failed to get calculation element");

			// Get the measurement name
			vName.Clear();
			pCalculationElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_CALC_NAME), &vName);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read calculation name");

			vUnits.Clear();
			pCalculationElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_UNITS), &vUnits);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed read calculation units");

			CString csUnits("");
			if (VT_EMPTY != V_VT(&vUnits))
				csUnits = V_BSTR(&vUnits);
			if (csUnits.IsEmpty())
				csUnits = L"none";

			// Get the embryonic information
			int iHorn = 0;
			vEmbryoHorn.Clear();
			pCalculationElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_EMBRYO_HORN), &vEmbryoHorn);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed read embryo horn");
			hr = VariantChangeTypeEx(
				&vEmbryoHorn, &vEmbryoHorn,
				MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
				0, VT_I4);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
			iHorn = V_I4(&vEmbryoHorn);

			int iEmbryo = 0;
			vEmbryoIndex.Clear();
			pCalculationElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_EMBRYO_INDEX), &vEmbryoIndex);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed read embryo index");
			hr = VariantChangeTypeEx(
				&vEmbryoIndex, &vEmbryoIndex,
				MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
				0, VT_I4);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
			iEmbryo = V_I4(&vEmbryoIndex);

			WCHAR szTempName[80];
			if (m_bFemale && m_bPregnant && iHorn > 0)	// Add embryo information if relavant
			{
				swprintf_s(szTempName, _countof(szTempName), L"%s : %cE : %d",
					V_BSTR(&vName), (iHorn == 1) ? L'L' : L'R', iEmbryo);
			}
			else
			{				
				wcscpy_s(szTempName, _countof(szTempName), V_BSTR(&vName));
			}

			strTemp.Format(L"\"%s\"%s%s\"%s\"%s", szTempName, m_szSptr, m_szSptr, csUnits, m_szSptr);
			strOut += strTemp;

			// Add value to output
			vValue.Clear();
			hr = pCalculationElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_CALC_VALUE), &vValue);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read calculation value");

			swprintf_s(szValue, _countof(szValue), V_BSTR(&vValue));
			if (iPrecision >= 0)
			{
				int iRet = VsiGetDoubleFormat(szValue, szValue, _countof(szValue), iPrecision);
				VSI_WIN32_VERIFY(0 != iRet, VSI_LOG_ERROR, NULL);
			}

			strTemp.Format(L"%s", szValue);
			strOut += strTemp;
			strOut += m_szSptr;

			strOut += L"\r\n";
		}
	}
	VSI_CATCH(hr);

	return hr;
}

/// <Summary>
///		Convert the Protocol element to CSV.
/// </Summary>
///
/// <param name = "CString& strOut">Pointer to output string</param>
///
/// <Returns>HRESULT : </Returns>
HRESULT CVsiCSVWriter::ConvertProtocolSection(CString& strOut, IXMLDOMElement *pPkgElement)
{
	HRESULT hr = S_OK;

	try
	{
		if ((VSI_CSV_WRITER_FILTER_MEASUREMENTS | VSI_CSV_WRITER_FILTER_CALCULATIONS) & m_filter)
		{
			strOut += L"\r\n";

			// Measurement File
			hr = ConvertPackageInfo(strOut, pPkgElement);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

			CComPtr<IXMLDOMNodeList> pProtocolList;
			hr = pPkgElement->selectNodes(CComBSTR(VSI_MSMNT_XML_ELE_PROTOCOL), &pProtocolList);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get protocol list");

			long lNumProtocols(0);
			hr = pProtocolList->get_length(&lNumProtocols);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get number of protocols");

			for (long lProtocolIndex = 0; lProtocolIndex < lNumProtocols; ++lProtocolIndex)
			{
				CComPtr<IXMLDOMNode> pProcotolNode;
				hr = pProtocolList->get_item(lProtocolIndex, &pProcotolNode);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get protocol node");

				CComQIPtr<IXMLDOMElement> pProtocolElement(pProcotolNode);
				VSI_CHECK_INTERFACE(pProtocolElement, VSI_LOG_ERROR, L"Failed to get protocol element");

				CComPtr<IXMLDOMNodeList> pMeasurementList;
				hr = pProtocolElement->selectNodes(
					CComBSTR(VSI_MSMNT_REPORT_XML_ELE_MSMNTS L"/" VSI_MSMNT_REPORT_XML_ELE_MSMNT),
					&pMeasurementList);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get measurement list");

				long lNumMeasurements(0);
				hr = pMeasurementList->get_length(&lNumMeasurements);
				VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get number of measurements");

				// If there are measurements under the protocol then go ahead
				if (lNumMeasurements > 0)
				{
					// Get protocol name
					CComVariant vProtocolName;
					hr = pProtocolElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PROTOCOL_NAME), &vProtocolName);
					VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to read protocol name");

					CString strProtocolName;
					strProtocolName.Format(L"\"Protocol Name\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vProtocolName)));
					strOut += strProtocolName;

					if (VSI_CSV_WRITER_FILTER_MEASUREMENTS & m_filter)
					{
						hr = ConvertProtocolMeasurements(strOut, pProtocolElement, FALSE);
						VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to convert protocol measurements");
					}

					if (VSI_CSV_WRITER_FILTER_CALCULATIONS & m_filter)
					{
						hr = ConvertProtocolCalculations(strOut, pProtocolElement);
						VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to convert protocol calculations");
					}

					ADD_NEWLINE_TO_STRING();
				}
			}

			if (VSI_CSV_WRITER_FILTER_MEASUREMENTS & m_filter)
			{
				if (0 == lNumProtocols)
				{
					ReportNoMsmnts(strOut);
				}
			}
		}
	}
	VSI_CATCH(hr);

	return hr;
}

/// <Summary>
///	Converts the specified intermediate XML file to the final CSV report file.
/// </Summary>
///
/// <Param name="pXmlDoc">Source XML doc</Param>
/// <Param name="pDestinationFile">Destination file name</Param>
/// <Returns></Returns>
HRESULT CVsiCSVWriter::ConvertFile(
	IXMLDOMDocument *pXmlDoc,
	IVsiApp *pApp,
	VSI_CSV_WRITER_FILTER filter,
	LPCWSTR pDestinationFile)
{
	HRESULT hr = S_OK;
	HANDLE hFile = NULL;

	try
	{
		VSI_CHECK_ARG(pXmlDoc, VSI_LOG_ERROR, NULL);
		VSI_CHECK_ARG(pApp, VSI_LOG_ERROR, NULL);
		VSI_CHECK_ARG(pDestinationFile, VSI_LOG_ERROR, NULL);

		m_pApp = pApp;
		m_filter = filter;

		CComQIPtr<IVsiServiceProvider> pServiceProvider(pApp);

		hr = pServiceProvider->QueryService(
			VSI_SERVICE_PDM,
			__uuidof(IVsiPdm),
			(IUnknown**)&m_pPdm);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		hr = pServiceProvider->QueryService(
			VSI_SERVICE_MODE_MANAGER,
			__uuidof(IVsiModeManager),
			(IUnknown**)&m_pModeMgr);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
		
		hr = GetFieldSeparator();
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to convert intermediate XML file");

		// Create the target file.
		hFile = CreateFile(pDestinationFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
		VSI_WIN32_VERIFY(hFile != INVALID_HANDLE_VALUE, VSI_LOG_ERROR | VSI_LOG_NO_BOX, L"Failure creating export file");

		m_pDoc = pXmlDoc;
		VSI_CHECK_INTERFACE(m_pDoc, VSI_LOG_ERROR, L"Failed to convert intermediate XML file");

		hr = ConvertHeaderSection(hFile);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to write file header");

		CComPtr<IXMLDOMNodeList> pStudyNodeList;
		hr = m_pDoc->getElementsByTagName(VSI_MSMNT_REPORT_XML_ELE_STUDY, &pStudyNodeList);
		
		long lNumStudies(0);
		hr = pStudyNodeList->get_length(&lNumStudies);
		for(int i = 0; i < lNumStudies; ++i)
		{
			CComPtr<IXMLDOMNode> pStudyNode;
			hr = pStudyNodeList->get_item(i, &pStudyNode);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, L"Failed to get study node");

			CComQIPtr<IXMLDOMElement> pStudyElement(pStudyNode);
			VSI_CHECK_PTR(pStudyElement, VSI_LOG_ERROR, L"Failed to get study element");

			hr = ConvertStudy(hFile, pStudyElement, TRUE);
			VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);
		}
	}
	VSI_CATCH_(err)
	{
		hr = err;
		if (FAILED(hr))
			VSI_ERROR_LOG(err);

		if (NULL != hFile)
		{
			CloseHandle(hFile);
			hFile = NULL;
			DeleteFile(pDestinationFile);
		}
	}

	if (NULL != hFile)
	{
		CloseHandle(hFile);
		hFile = NULL;
	}

	m_pDoc.Release();
	m_pApp.Release();
	m_pPdm.Release();
	m_pModeMgr.Release();

	return hr;
}

HRESULT CVsiCSVWriter::ConvertPackageInfo(CString& strOut, IXMLDOMElement *pPkgElement)
{
	HRESULT hr = S_OK;
	CString strTemp;

	try
	{
		CComVariant vPackageFile;
		hr = pPkgElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PACKAGE_FILE), &vPackageFile);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		strTemp.Format(L"\"Measurement File\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vPackageFile)));
		strOut += strTemp;

		CComVariant vPackageVersion;
		hr = pPkgElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PACKAGE_VERSION), &vPackageVersion);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		strTemp.Format(L"\"Measurement Version\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vPackageVersion)));
		strOut += strTemp;

		CComVariant vPackageName;
		hr = pPkgElement->getAttribute(CComBSTR(VSI_MSMNT_XML_ATT_PACKAGE_NAME), &vPackageName);
		VSI_CHECK_HR(hr, VSI_LOG_ERROR, NULL);

		strTemp.Format(L"\"Measurement Description\"%s%s\r\n", m_szSptr, (LPCWSTR)FormatStringForCSV(V_BSTR(&vPackageName)));
		strOut += strTemp;
		ADD_NEWLINE_TO_STRING();
	}
	VSI_CATCH(hr);

	return hr;
}

void CVsiCSVWriter::ReportNoMsmnts(CString& strOut)
{
	strOut += "No measurements found\r\n\r\n\r\n";
}

void CVsiCSVWriter::FormatVariantDouble(CComVariant &vNum, int iPrecision) throw(...)
{
	if (VT_R8 != V_VT(&vNum))
	{
		HRESULT hr = VariantChangeTypeEx(
			&vNum, &vNum,
			MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), SORT_DEFAULT),
			0, VT_R8);
		if (S_OK != hr)
		{
			// Assume it is not a real number (e.g. N/A) 
			return;
		}
	}

	WCHAR szNum[500];
	int iRet = swprintf_s(szNum, L"%f", V_R8(&vNum));
	VSI_VERIFY(-1 != iRet, VSI_LOG_ERROR, NULL);

	if (0 == iPrecision)
	{
		iPrecision = 6;
	}

	iRet = VsiGetDoubleFormat(szNum, szNum, _countof(szNum), iPrecision);
	VSI_WIN32_VERIFY(0 != iRet, VSI_LOG_ERROR, NULL);

	vNum = szNum;
}
