/*

DISKSPD

Copyright(c) Microsoft Corporation
All rights reserved.

MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED *AS IS*, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "XmlProfileParser.h"
#include <Objbase.h>
#include <msxml6.h>
#include <assert.h>

// the vc com headers define a partial set of smartptr typedefs, which unfortunately
// aren't 1) complete for our use case and 2) vary between revs of the headers.
// this define disables the automatic definitions, letting us do them ourselves.
#define _COM_NO_STANDARD_GUIDS_
#include <comdef.h>

_COM_SMARTPTR_TYPEDEF(IXMLDOMDocument2, __uuidof(IXMLDOMDocument2));
_COM_SMARTPTR_TYPEDEF(IXMLDOMSchemaCollection2, __uuidof(IXMLDOMSchemaCollection2)); 
_COM_SMARTPTR_TYPEDEF(IXMLDOMNode, __uuidof(IXMLDOMNodeList));
_COM_SMARTPTR_TYPEDEF(IXMLDOMNodeList, __uuidof(IXMLDOMNodeList));
_COM_SMARTPTR_TYPEDEF(IXMLDOMNamedNodeMap, __uuidof(IXMLDOMNamedNodeMap));
_COM_SMARTPTR_TYPEDEF(IXMLDOMParseError, __uuidof(IXMLDOMParseError));

HRESULT ReportXmlError(
	const char *pszName,
	IXMLDOMParseErrorPtr pXMLError
	)
{
	long line;
	long linePos;
	long errorCode = E_FAIL;
	_bstr_t bReason;
	BSTR bstr;
	HRESULT hr;

	hr = pXMLError->get_line(&line);
	if (FAILED(hr))
	{
		line = 0;
	}
	hr = pXMLError->get_linepos(&linePos);
	if (FAILED(hr))
	{
		linePos = 0;
	}
	hr = pXMLError->get_errorCode(&errorCode);
	if (FAILED(hr))
	{
		errorCode = E_FAIL;
	}
	hr = pXMLError->get_reason(&bstr);
	if (SUCCEEDED(hr))
	{
		bReason.Attach(bstr);
	}

	fprintf(stderr,
		"ERROR: failed to load %s, line %lu, line position %lu, errorCode %08x\nERROR: reason: %S\n",
		pszName, line, linePos, errorCode, (PWCHAR)bReason);

	return errorCode;
}

bool XmlProfileParser::ParseFile(const char *pszPath, Profile *pProfile)
{
    assert(pszPath != nullptr);
    assert(pProfile != nullptr);

    // import schema from the named resource
    HRSRC hSchemaXmlResource = FindResource(NULL, L"DISKSPD.XSD", RT_HTML);
    assert(hSchemaXmlResource != NULL);
    HGLOBAL hSchemaXml = LoadResource(NULL, hSchemaXmlResource);
    assert(hSchemaXml != NULL);
    LPVOID pSchemaXml = LockResource(hSchemaXml);
    assert(pSchemaXml != NULL);
    
    // convert from utf-8 produced by the xsd authoring tool to utf-16
    int cchSchemaXml = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)pSchemaXml, -1, NULL, 0);
	vector<WCHAR> vWideSchemaXml(cchSchemaXml);
    int dwcchWritten = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)pSchemaXml, -1, vWideSchemaXml.data(), cchSchemaXml);
    UNREFERENCED_PARAMETER(dwcchWritten);
    assert(dwcchWritten == cchSchemaXml);
    // ... and finally, packed in a bstr for the loadXml interface
    _bstr_t bSchemaXml(vWideSchemaXml.data());

    bool fComInitialized = false;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
        fComInitialized = true;
        IXMLDOMDocument2Ptr spXmlDoc = nullptr;
        IXMLDOMDocument2Ptr spXmlSchema = nullptr;
        IXMLDOMSchemaCollection2Ptr spXmlSchemaColl = nullptr;
        IXMLDOMParseErrorPtr spXmlParseError = nullptr;

        // create com objects and decorate
        hr = CoCreateInstance(__uuidof(DOMDocument60), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spXmlSchema));
        if (SUCCEEDED(hr))
        {
            hr = spXmlSchema->put_async(VARIANT_FALSE);
        }
        if (SUCCEEDED(hr))
        {
            hr = spXmlSchema->setProperty(_bstr_t("ProhibitDTD").GetBSTR(), _variant_t(VARIANT_FALSE));
        }
        if (SUCCEEDED(hr))
        {
            hr = CoCreateInstance(__uuidof(XMLSchemaCache60), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spXmlSchemaColl));
        }
        if (SUCCEEDED(hr))
        {
            hr = spXmlSchemaColl->put_validateOnLoad(VARIANT_TRUE);
        }
        if (SUCCEEDED(hr))
        {
            hr = CoCreateInstance(__uuidof(DOMDocument60), nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&spXmlDoc));
        }
        if (SUCCEEDED(hr))
        {
            hr = spXmlDoc->put_async(VARIANT_FALSE);
        }
        if (SUCCEEDED(hr))
        {
            hr = spXmlDoc->put_validateOnParse(VARIANT_TRUE);
        }
        // work in progress to complete XML schema validation
        // load schema and attach to schema collection, attach schema collection to spec doc, then load specification
#if 0
		//
		// Issue at the moment: load fails with error as follows.
		// ERROR: failed to load schema, line 1, line position 1, errorCode c00ce556
		// ERROR: reason : Invalid at the top level of the document.
        if (SUCCEEDED(hr))
        {
            VARIANT_BOOL fvIsOk;
            hr = spXmlSchema->loadXML(bSchemaXml.GetBSTR(), &fvIsOk);
            if (SUCCEEDED(hr) && fvIsOk != VARIANT_TRUE)
            {
                hr = spXmlSchema->get_parseError(&spXmlParseError);
                if (SUCCEEDED(hr))
                {
                    ReportXmlError("schema", spXmlParseError);
                }
                hr = E_FAIL;
            }
        }
		if (SUCCEEDED(hr))
        {
            _variant_t vXmlSchema(spXmlSchema);
            _bstr_t bNull("");
            hr = spXmlSchemaColl->add(bNull, vXmlSchema);
        }
        if (SUCCEEDED(hr))
        {
            _variant_t vSchemaCache(spXmlSchemaColl);
            hr = spXmlDoc->putref_schemas(vSchemaCache);
        }
#endif
        if (SUCCEEDED(hr))
        {
            VARIANT_BOOL fvIsOk;
            _variant_t vPath(pszPath);
            hr = spXmlDoc->load(vPath, &fvIsOk);
            if (SUCCEEDED(hr) && fvIsOk != VARIANT_TRUE)
            {
                hr = E_FAIL;
            }
        }

        // now parse the specification, if correct
        if (SUCCEEDED(hr))
        {
            bool fVerbose;
            hr = _GetVerbose(spXmlDoc, &fVerbose);
            if (SUCCEEDED(hr) && (hr != S_FALSE))
            {
                pProfile->SetVerbose(fVerbose);
            }

            if (SUCCEEDED(hr))
            {
                DWORD dwProgress;
                hr = _GetProgress(spXmlDoc, &dwProgress);
                if (SUCCEEDED(hr) && (hr != S_FALSE))
                {
                    pProfile->SetProgress(dwProgress);
                }
            }

            if (SUCCEEDED(hr))
            {
                string sResultFormat;
                hr = _GetString(spXmlDoc, "//Profile/ResultFormat", &sResultFormat);
                if (SUCCEEDED(hr) && (hr != S_FALSE) && sResultFormat == "xml")
                {
                    pProfile->SetResultsFormat(ResultsFormat::Xml);
                }
            }

            if (SUCCEEDED(hr))
            {
                string sCreateFiles;
                hr = _GetString(spXmlDoc, "//Profile/PrecreateFiles", &sCreateFiles);
                if (SUCCEEDED(hr) && (hr != S_FALSE))
                {
                    if (sCreateFiles == "UseMaxSize")
                    {
                        pProfile->SetPrecreateFiles(PrecreateFiles::UseMaxSize);
                    }
                    else if (sCreateFiles == "CreateOnlyFilesWithConstantSizes")
                    {
                        pProfile->SetPrecreateFiles(PrecreateFiles::OnlyFilesWithConstantSizes);
                    }
                    else if (sCreateFiles == "CreateOnlyFilesWithConstantOrZeroSizes")
                    {
                        pProfile->SetPrecreateFiles(PrecreateFiles::OnlyFilesWithConstantOrZeroSizes);
                    }
                    else
                    {
                        hr = E_INVALIDARG;
                    }
                }
            }

            if (SUCCEEDED(hr))
            {
                hr = _ParseEtw(spXmlDoc, pProfile);
            }

            if (SUCCEEDED(hr))
            {
                hr = _ParseTimeSpans(spXmlDoc, pProfile);
            }
        }
    }
    if (fComInitialized)
    {
        CoUninitialize();
    }

    return SUCCEEDED(hr);
}

HRESULT XmlProfileParser::_ParseEtw(IXMLDOMDocument2 &XmlDoc, Profile *pProfile)
{
    bool fEtwProcess;
    HRESULT hr = _GetBool(XmlDoc, "//Profile/ETW/Process", &fEtwProcess);
    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        pProfile->SetEtwEnabled(true);
        pProfile->SetEtwProcess(fEtwProcess);
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwThread;
        hr = _GetBool(XmlDoc, "//Profile/ETW/Thread", &fEtwThread);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwThread(fEtwThread);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwImageLoad;
        hr = _GetBool(XmlDoc, "//Profile/ETW/ImageLoad", &fEtwImageLoad);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwImageLoad(fEtwImageLoad);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwDiskIO;
        hr = _GetBool(XmlDoc, "//Profile/ETW/DiskIO", &fEtwDiskIO);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwDiskIO(fEtwDiskIO);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwMemoryPageFaults;
        hr = _GetBool(XmlDoc, "//Profile/ETW/MemoryPageFaults", &fEtwMemoryPageFaults);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwMemoryPageFaults(fEtwMemoryPageFaults);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwMemoryHardFaults;
        hr = _GetBool(XmlDoc, "//Profile/ETW/MemoryHardFaults", &fEtwMemoryHardFaults);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwMemoryHardFaults(fEtwMemoryHardFaults);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwNetwork;
        hr = _GetBool(XmlDoc, "//Profile/ETW/Network", &fEtwNetwork);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwNetwork(fEtwNetwork);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwRegistry;
        hr = _GetBool(XmlDoc, "//Profile/ETW/Registry", &fEtwRegistry);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwRegistry(fEtwRegistry);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwUsePagedMemory;
        hr = _GetBool(XmlDoc, "//Profile/ETW/UsePagedMemory", &fEtwUsePagedMemory);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwUsePagedMemory(fEtwUsePagedMemory);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwUsePerfTimer;
        hr = _GetBool(XmlDoc, "//Profile/ETW/UsePerfTimer", &fEtwUsePerfTimer);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwUsePerfTimer(fEtwUsePerfTimer);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwUseSystemTimer;
        hr = _GetBool(XmlDoc, "//Profile/ETW/UseSystemTimer", &fEtwUseSystemTimer);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwUseSystemTimer(fEtwUseSystemTimer);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fEtwUseCyclesCounter;
        hr = _GetBool(XmlDoc, "//Profile/ETW/UseCyclesCounter", &fEtwUseCyclesCounter);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pProfile->SetEtwEnabled(true);
            pProfile->SetEtwUseCyclesCounter(fEtwUseCyclesCounter);
        }
    }

    return hr;
}

HRESULT XmlProfileParser::_ParseTimeSpans(IXMLDOMDocument2 &XmlDoc, Profile *pProfile)
{
    IXMLDOMNodeListPtr spNodeList;
    _variant_t query("//Profile/TimeSpans/TimeSpan");
    HRESULT hr = XmlDoc.selectNodes(query.bstrVal, &spNodeList);
    if (SUCCEEDED(hr))
    {
        long cNodes;
        hr = spNodeList->get_length(&cNodes);
        if (SUCCEEDED(hr))
        {
            for (int i = 0; i < cNodes; i++)
            {
                IXMLDOMNodePtr spNode;
                hr = spNodeList->get_item(i, &spNode);
                if (SUCCEEDED(hr))
                {
                    TimeSpan timeSpan;
                    hr = _ParseTimeSpan(spNode, &timeSpan);
                    if (SUCCEEDED(hr))
                    {
                        pProfile->AddTimeSpan(timeSpan);
                    }
                }
            }
        }
    }

    return hr;
}

HRESULT XmlProfileParser::_ParseTimeSpan(IXMLDOMNode &XmlNode, TimeSpan *pTimeSpan)
{
    UINT32 ulDuration;
    HRESULT hr = _GetUINT32(XmlNode, "Duration", &ulDuration);
    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        pTimeSpan->SetDuration(ulDuration);
    }

    if (SUCCEEDED(hr))
    {
        UINT32 ulWarmup;
        hr = _GetUINT32(XmlNode, "Warmup", &ulWarmup);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTimeSpan->SetWarmup(ulWarmup);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT32 ulCooldown;
        hr = _GetUINT32(XmlNode, "Cooldown", &ulCooldown);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTimeSpan->SetCooldown(ulCooldown);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT32 ulRandSeed;
        hr = _GetUINT32(XmlNode, "RandSeed", &ulRandSeed);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTimeSpan->SetRandSeed(ulRandSeed);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT32 ulThreadCount;
        hr = _GetUINT32(XmlNode, "ThreadCount", &ulThreadCount);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTimeSpan->SetThreadCount(ulThreadCount);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fDisableAffinity;
        hr = _GetBool(XmlNode, "DisableAffinity", &fDisableAffinity);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTimeSpan->SetDisableAffinity(fDisableAffinity);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fCompletionRoutines;
        hr = _GetBool(XmlNode, "CompletionRoutines", &fCompletionRoutines);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTimeSpan->SetCompletionRoutines(fCompletionRoutines);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fMeasureLatency;
        hr = _GetBool(XmlNode, "MeasureLatency", &fMeasureLatency);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTimeSpan->SetMeasureLatency(fMeasureLatency);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fCalculateIopsStdDev;
        hr = _GetBool(XmlNode, "CalculateIopsStdDev", &fCalculateIopsStdDev);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTimeSpan->SetCalculateIopsStdDev(fCalculateIopsStdDev);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT32 ulIoBucketDuration;
        hr = _GetUINT32(XmlNode, "IoBucketDuration", &ulIoBucketDuration);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTimeSpan->SetIoBucketDurationInMilliseconds(ulIoBucketDuration);
        }
    }

    // Look for downlevel non-group aware assignment
    if (SUCCEEDED(hr))
    {
        hr = _ParseAffinityAssignment(XmlNode, pTimeSpan);
    }

    // Look for uplevel group aware assignment.
    if (SUCCEEDED(hr))
    {
        hr = _ParseAffinityGroupAssignment(XmlNode, pTimeSpan);
    }

    if (SUCCEEDED(hr))
    {
        hr = _ParseTargets(XmlNode, pTimeSpan);
    }
    return hr;
}

HRESULT XmlProfileParser::_ParseTargets(IXMLDOMNode &XmlNode, TimeSpan *pTimeSpan)
{
    _variant_t query("Targets/Target");
    IXMLDOMNodeListPtr spNodeList;
    HRESULT hr = XmlNode.selectNodes(query.bstrVal, &spNodeList);
    if (SUCCEEDED(hr))
    {
        long cNodes;
        hr = spNodeList->get_length(&cNodes);
        if (SUCCEEDED(hr))
        {
            for (int i = 0; i < cNodes; i++)
            {
                IXMLDOMNodePtr spNode;
                hr = spNodeList->get_item(i, &spNode);
                if (SUCCEEDED(hr))
                {
                    Target target;
                    _ParseTarget(spNode, &target);
                    pTimeSpan->AddTarget(target);
                }
            }
        }
    }
    return hr;
}

HRESULT XmlProfileParser::_ParseRandomDataSource(IXMLDOMNode &XmlNode, Target *pTarget)
{
    IXMLDOMNodeListPtr spNodeList;
    _variant_t query("RandomDataSource");
    HRESULT hr = XmlNode.selectNodes(query.bstrVal, &spNodeList);
    if (SUCCEEDED(hr))
    {
        long cNodes;
        hr = spNodeList->get_length(&cNodes);
        if (SUCCEEDED(hr) && (cNodes == 1))
        {
            IXMLDOMNodePtr spNode;
            hr = spNodeList->get_item(0, &spNode);
            if (SUCCEEDED(hr))
            {
                UINT64 cb;
                hr = _GetUINT64(spNode, "SizeInBytes", &cb);
                if (SUCCEEDED(hr) && (S_FALSE != hr))
                {
                    pTarget->SetRandomDataWriteBufferSize(cb);

                    string sPath;
                    hr = _GetString(spNode, "FilePath", &sPath);
                    if (SUCCEEDED(hr) && (S_FALSE != hr))
                    {
                        pTarget->SetRandomDataWriteBufferSourcePath(sPath);
                    }
                }
            }
        }
    }
    return hr;
}

HRESULT XmlProfileParser::_ParseWriteBufferContent(IXMLDOMNode &XmlNode, Target *pTarget)
{
    IXMLDOMNodeListPtr spNodeList;
    _variant_t query("WriteBufferContent");
    HRESULT hr = XmlNode.selectNodes(query.bstrVal, &spNodeList);
    if (SUCCEEDED(hr))
    {
        long cNodes;
        hr = spNodeList->get_length(&cNodes);
        if (SUCCEEDED(hr) && (cNodes == 1))
        {
            IXMLDOMNodePtr spNode;
            hr = spNodeList->get_item(0, &spNode);
            if (SUCCEEDED(hr))
            {
                string sPattern;
                hr = _GetString(spNode, "Pattern", &sPattern);
                if (SUCCEEDED(hr) && (hr != S_FALSE))
                {
                    if (sPattern == "sequential")
                    {
                        // that's the default option - do nothing
                    }
                    else if (sPattern == "zero")
                    {
                        pTarget->SetZeroWriteBuffers(true);
                    }
                    else if (sPattern == "random")
                    {
                        hr = _ParseRandomDataSource(spNode, pTarget);
                    }
                    else
                    {
                        hr = E_INVALIDARG;
                    }
                }
            }
        }
    }
    return hr;
}

HRESULT XmlProfileParser::_ParseTarget(IXMLDOMNode &XmlNode, Target *pTarget)
{
    string sPath;
    HRESULT hr = _GetString(XmlNode, "Path", &sPath);
    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        pTarget->SetPath(sPath);
    }

    if (SUCCEEDED(hr))
    {
        DWORD dwBlockSize;
        hr = _GetDWORD(XmlNode, "BlockSize", &dwBlockSize);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetBlockSizeInBytes(dwBlockSize);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT64 ullStrideSize;
        hr = _GetUINT64(XmlNode, "StrideSize", &ullStrideSize);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetBlockAlignmentInBytes(ullStrideSize);
        }
    }
    
    if (SUCCEEDED(hr))
    {
        bool fInterlockedSequential;
        hr = _GetBool(XmlNode, "InterlockedSequential", &fInterlockedSequential);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetUseInterlockedSequential(fInterlockedSequential);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT64 ullBaseFileOffset;
        hr = _GetUINT64(XmlNode, "BaseFileOffset", &ullBaseFileOffset);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetBaseFileOffsetInBytes(ullBaseFileOffset);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fBool;
        hr = _GetBool(XmlNode, "SequentialScan", &fBool);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetSequentialScanHint(fBool);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fBool;
        hr = _GetBool(XmlNode, "RandomAccess", &fBool);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetRandomAccessHint(fBool);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fBool;
        hr = _GetBool(XmlNode, "TemporaryFile", &fBool);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetTemporaryFileHint(fBool);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fUseLargePages;
        hr = _GetBool(XmlNode, "UseLargePages", &fUseLargePages);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetUseLargePages(fUseLargePages);
        }
    }

    if (SUCCEEDED(hr))
    {
        DWORD dwRequestCount;
        hr = _GetDWORD(XmlNode, "RequestCount", &dwRequestCount);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetRequestCount(dwRequestCount);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT64 ullRandom;
        hr = _GetUINT64(XmlNode, "Random", &ullRandom);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetUseRandomAccessPattern(true);
            pTarget->SetBlockAlignmentInBytes(ullRandom);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fBool;
        hr = _GetBool(XmlNode, "DisableOSCache", &fBool);
        if (SUCCEEDED(hr) && (hr != S_FALSE) && fBool)
        {
            pTarget->SetCacheMode(TargetCacheMode::DisableOSCache);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fBool;
        hr = _GetBool(XmlNode, "DisableAllCache", &fBool);
        if (SUCCEEDED(hr) && (hr != S_FALSE) && fBool)
        {
            pTarget->SetCacheMode(TargetCacheMode::DisableAllCache);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fBool;
        hr = _GetBool(XmlNode, "DisableLocalCache", &fBool);
        if (SUCCEEDED(hr) && (hr != S_FALSE) && fBool)
        {
            pTarget->SetCacheMode(TargetCacheMode::DisableLocalCache);
        }
    }

    if (SUCCEEDED(hr))
    {
        hr = _ParseWriteBufferContent(XmlNode, pTarget);
    }

    if (SUCCEEDED(hr))
    {
        DWORD dwBurstSize;
        hr = _GetDWORD(XmlNode, "BurstSize", &dwBurstSize);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetBurstSize(dwBurstSize);
            pTarget->SetUseBurstSize(true);
        }
    }

    if (SUCCEEDED(hr))
    {
        DWORD dwThinkTime;
        hr = _GetDWORD(XmlNode, "ThinkTime", &dwThinkTime);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetThinkTime(dwThinkTime);
            pTarget->SetEnableThinkTime(true);
        }
    }

    if (SUCCEEDED(hr))
    {
        DWORD dwThroughput;
        hr = _GetDWORD(XmlNode, "Throughput", &dwThroughput);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetThroughput(dwThroughput);
        }
    }

    if (SUCCEEDED(hr))
    {
        DWORD dwThreadsPerFile;
        hr = _GetDWORD(XmlNode, "ThreadsPerFile", &dwThreadsPerFile);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetThreadsPerFile(dwThreadsPerFile);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT64 ullFileSize;
        hr = _GetUINT64(XmlNode, "FileSize", &ullFileSize);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetFileSize(ullFileSize);
            pTarget->SetCreateFile(true);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT64 ullMaxFileSize;
        hr = _GetUINT64(XmlNode, "MaxFileSize", &ullMaxFileSize);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetMaxFileSize(ullMaxFileSize);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT32 ulWriteRatio;
        hr = _GetUINT32(XmlNode, "WriteRatio", &ulWriteRatio);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetWriteRatio(ulWriteRatio);
        }
    }

    if (SUCCEEDED(hr))
    {
        bool fParallelAsyncIO;
        hr = _GetBool(XmlNode, "ParallelAsyncIO", &fParallelAsyncIO);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetUseParallelAsyncIO(fParallelAsyncIO);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT64 ullThreadStride;
        hr = _GetUINT64(XmlNode, "ThreadStride", &ullThreadStride);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            pTarget->SetThreadStrideInBytes(ullThreadStride);
        }
    }

    if (SUCCEEDED(hr))
    {
        UINT32 ulIOPriority;
        hr = _GetUINT32(XmlNode, "IOPriority", &ulIOPriority);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            PRIORITY_HINT hint[] = { IoPriorityHintVeryLow, IoPriorityHintLow, IoPriorityHintNormal };
            pTarget->SetIOPriorityHint(hint[ulIOPriority - 1]);
        }
    }
    return hr;
}

// Compatibility with the old, non-group aware affinity assignment. Preserved to allow downlevel XML profiles
// to run without modification.
// Any assignment done through this method will only assign within group 0, and is equivalent to the non-group
// specification -a#,#,# (contrast to -ag#,#,#,...). While not strictly equivalent to the old non-group aware
// behavior, this should be acceptably good-enough.
//
// The XML result parser no longer emits this form.

HRESULT XmlProfileParser::_ParseAffinityAssignment(IXMLDOMNode &XmlNode, TimeSpan *pTimeSpan)
{
    IXMLDOMNodeListPtr spNodeList;
    _variant_t query("Affinity/AffinityAssignment");
    HRESULT hr = XmlNode.selectNodes(query.bstrVal, &spNodeList);
    if (SUCCEEDED(hr))
    {
        long cNodes;
        hr = spNodeList->get_length(&cNodes);
        if (SUCCEEDED(hr))
        {
            for (int i = 0; i < cNodes; i++)
            {
                IXMLDOMNodePtr spNode;
                hr = spNodeList->get_item(i, &spNode);
                if (SUCCEEDED(hr))
                {
                    BSTR bstrText;
                    hr = spNode->get_text(&bstrText);
                    if (SUCCEEDED(hr))
                    {
                        pTimeSpan->AddAffinityAssignment((WORD)0, (BYTE)_wtoi((wchar_t *)bstrText));
                        SysFreeString(bstrText);
                    }
                }
            }
        }
    }
    return hr;
}

// Group aware affinity assignment. This is the only form emitted by the XML result parser.

HRESULT XmlProfileParser::_ParseAffinityGroupAssignment(IXMLDOMNode &XmlNode, TimeSpan *pTimeSpan)
{
    IXMLDOMNodeListPtr spNodeList;
    _variant_t query("Affinity/AffinityGroupAssignment");

    HRESULT hr = XmlNode.selectNodes(query.bstrVal, &spNodeList);
    if (SUCCEEDED(hr))
    {
        long cNodes;
        hr = spNodeList->get_length(&cNodes);
        if (SUCCEEDED(hr))
        {
            for (int i = 0; i < cNodes; i++)
            {
                IXMLDOMNodePtr spNode;
                hr = spNodeList->get_item(i, &spNode);
                if (SUCCEEDED(hr))
                {
                    UINT32 dwGroup = 0, dwProc = 0;
                    hr = _GetUINT32Attr(spNode, "Group", &dwGroup);
                    if (SUCCEEDED(hr))
                    {
                        _GetUINT32Attr(spNode, "Processor", &dwProc);
                    }
                    if (SUCCEEDED(hr))
                    {
                        if (dwProc > MAXBYTE)
                        {
                            fprintf(stderr, "ERROR: profile specifies group assignment to core %u, out of range\n", dwProc);
                            hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                        }
                        if (dwGroup > MAXWORD)
                        {
                            fprintf(stderr, "ERROR: profile specifies group assignment group %u, out of range\n", dwGroup);
                            hr = HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
                        }

                        if (SUCCEEDED(hr)) {
                            pTimeSpan->AddAffinityAssignment((WORD)dwGroup, (BYTE)dwProc);
                        }
                        
                    }
                }
            }
        }
    }
    return hr;
}

HRESULT XmlProfileParser::_GetUINT32(IXMLDOMNode &XmlNode, const char *pszQuery, UINT32 *pulValue) const
{
    IXMLDOMNodePtr spNode;
    _variant_t query(pszQuery);
    HRESULT hr = XmlNode.selectSingleNode(query.bstrVal, &spNode);
    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        BSTR bstrText;
        hr = spNode->get_text(&bstrText);
        if (SUCCEEDED(hr))
        {
            *pulValue = _wtoi((wchar_t *)bstrText);  // TODO: make sure it works on large unsigned ints
            SysFreeString(bstrText);
        }
    }
    return hr;
}

HRESULT XmlProfileParser::_GetUINT32Attr(IXMLDOMNode &XmlNode, const char *pszAttr, UINT32 *pulValue) const
{
    IXMLDOMNamedNodeMapPtr spNamedNodeMap;
    _bstr_t attr(pszAttr);
    HRESULT hr = XmlNode.get_attributes(&spNamedNodeMap);
    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        IXMLDOMNodePtr spNode;
        HRESULT hr = spNamedNodeMap->getNamedItem(attr, &spNode);
        if (SUCCEEDED(hr) && (hr != S_FALSE))
        {
            BSTR bstrText;
            hr = spNode->get_text(&bstrText);
            if (SUCCEEDED(hr))
            {
                *pulValue = _wtoi((wchar_t *)bstrText);  // TODO: make sure it works on large unsigned ints
                SysFreeString(bstrText);
            }
        }
    }
    return hr;
}

HRESULT XmlProfileParser::_GetString(IXMLDOMNode &XmlNode, const char *pszQuery, string *psValue) const
{
    IXMLDOMNodePtr spNode;
    _variant_t query(pszQuery);
    HRESULT hr = XmlNode.selectSingleNode(query.bstrVal, &spNode);
    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        BSTR bstrText;
        hr = spNode->get_text(&bstrText);
        if (SUCCEEDED(hr))
        {
            // TODO: use wstring?
            char path[MAX_PATH] = {};
            WideCharToMultiByte(CP_UTF8, 0 /*dwFlags*/, (wchar_t *)bstrText, static_cast<int>(wcslen((wchar_t *)bstrText)), path, sizeof(path)-1, 0 /*lpDefaultChar*/, 0 /*lpUsedDefaultChar*/);
            *psValue = string(path);
        }
        SysFreeString(bstrText);
    }
    return hr;
}

HRESULT XmlProfileParser::_GetUINT64(IXMLDOMNode &XmlNode, const char *pszQuery, UINT64 *pullValue) const
{
    IXMLDOMNodePtr spNode;
    _variant_t query(pszQuery);
    HRESULT hr = XmlNode.selectSingleNode(query.bstrVal, &spNode);
    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        BSTR bstrText;
        hr = spNode->get_text(&bstrText);
        if (SUCCEEDED(hr))
        {
            *pullValue = _wtoi64((wchar_t *)bstrText);  // TODO: make sure it works on large unsigned ints
        }
        SysFreeString(bstrText);
    }
    return hr;
}

HRESULT XmlProfileParser::_GetDWORD(IXMLDOMNode &XmlNode, const char *pszQuery, DWORD *pdwValue) const
{
    UINT32 value = 0;
    HRESULT hr = _GetUINT32(XmlNode, pszQuery, &value);
    if (SUCCEEDED(hr))
    {
        *pdwValue = value;
    }
    return hr;
}

HRESULT XmlProfileParser::_GetBool(IXMLDOMNode &XmlNode, const char *pszQuery, bool *pfValue) const
{
    HRESULT hr = S_OK;
    IXMLDOMNodePtr spNode;
    _variant_t query(pszQuery);
    hr = XmlNode.selectSingleNode(query.bstrVal, &spNode);
    if (SUCCEEDED(hr) && (hr != S_FALSE))
    {
        BSTR bstrText;
        hr = spNode->get_text(&bstrText);
        if (SUCCEEDED(hr))
        {
            *pfValue = (_wcsicmp(L"true", (wchar_t *)bstrText) == 0);
            SysFreeString(bstrText);
        }
    }
    return hr;
}

HRESULT XmlProfileParser::_GetVerbose(IXMLDOMDocument2 &pXmlDoc, bool *pfVerbose)
{
    return _GetBool(pXmlDoc, "//Profile/Verbose", pfVerbose);
}

HRESULT XmlProfileParser::_GetProgress(IXMLDOMDocument2 &pXmlDoc, DWORD *pdwProgress)
{
    return _GetDWORD(pXmlDoc, "//Profile/Progress", pdwProgress);
}