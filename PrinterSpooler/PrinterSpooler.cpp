#include "PrinterSpooler.h"

#include <sstream>
#include <algorithm>
#pragma comment(lib, "Winspool.lib")

namespace Spooler
{
#define ONEINCH 2.54
#define A5_WIDTH (2450 / ONEINCH)
#define A5_HEIGHT (1750 / ONEINCH)
#define LETTER_WIDTH (2550 / ONEINCH)//(2650 / ONEINCH)
#define LETTER_HEIGHT (4000 / ONEINCH)//(3300 / ONEINCH)
#define MARGIN 30

	TCHAR	szDriver[16] = _T("WINSPOOL");

	PrinterSpooler::PrinterSpooler(const Spooler_Subscriber_Ptr& subscriber, const std::string& printerName) :
		m_subscriber(subscriber),
		m_printerName(printerName),
		m_openSuccess(false),
		m_readTerminated(true)
	{
	}

	PrinterSpooler::~PrinterSpooler()
	{
	}

	void PrinterSpooler::initializePrinterSpooler()
	{
		//for listening the printer status!
		if (OpenPrinter(const_cast<char *>(m_printerName.c_str()), &m_HPrinterSpooler, NULL) == TRUE &&
			m_HPrinterSpooler != INVALID_HANDLE_VALUE)
		{
			m_openSuccess = true;
			m_readTerminated = false;

			DWORD statusThreadID;

			updatePrinterStatus(); //to inform observer about the printer status at startup!

			m_HStatusThread = CreateThread(NULL, 0, PrinterSpooler::startMonitorPrinter, this, 0, &statusThreadID);
		}
		else
		{
			std::cout << "PrinterSpooler::initializePrinterSpooler() -> failed to get handle for printer spooler" << std::endl;
		}

		//for listening the status of jobs in the spooler!
		if (OpenPrinter(const_cast<char *>(m_printerName.c_str()), &m_HSpoolerJob, NULL) == TRUE &&
			m_HSpoolerJob != INVALID_HANDLE_VALUE)
		{
			DWORD jobThreadID;
			m_HJobStatusThread = CreateThread(NULL, 0, PrinterSpooler::startMonitorSpoolerJobs, this, 0, &jobThreadID);
		}
		else
		{
			std::cout << "PrinterSpooler::initializePrinterSpooler() -> failed to get handle for spooler job status" << std::endl;
		}
	}

	void PrinterSpooler::closePrinterSpooler()
	{
		m_openSuccess = false;
		m_readTerminated = true;
		m_jobStatusMap.clear();

		CloseHandle(m_HPrinterSpooler);
		CloseHandle(m_HSpoolerJob);
		CloseHandle(m_HStatusThread);
		CloseHandle(m_HJobStatusThread);
		CloseHandle(m_HJobNotify);
		CloseHandle(m_HStatusNotify);
	}

	DWORD WINAPI PrinterSpooler::startMonitorPrinter(LPVOID lpV)
	{
		return static_cast<PrinterSpooler*>(lpV)->MonitorPrinter();
	}

	DWORD WINAPI PrinterSpooler::startMonitorSpoolerJobs(LPVOID lpV)
	{
		return static_cast<PrinterSpooler*>(lpV)->MonitorSpoolerJobs();
	}

	DWORD PrinterSpooler::MonitorPrinter()
	{
		std::cout << "PrinterSpooler::MonitorPrinter()" << std::endl;

		PRINTER_NOTIFY_OPTIONS* notifyOptions;
		PPRINTER_NOTIFY_INFO notifyInfo;
		DWORD dwChanged;
		PRINTER_NOTIFY_OPTIONS_TYPE rOptions[1];
		bool m_Exited = false;

		WORD wPrinterFields[2];
		wPrinterFields[0] = PRINTER_NOTIFY_FIELD_STATUS;
		wPrinterFields[1] = PRINTER_NOTIFY_FIELD_ATTRIBUTES;

		rOptions[0].Type = PRINTER_NOTIFY_TYPE;
		rOptions[0].pFields = &wPrinterFields[0];
		rOptions[0].Count = 2;

		notifyOptions = new PRINTER_NOTIFY_OPTIONS;
		notifyOptions->Count = 1;
		notifyOptions->Version = 2;
		notifyOptions->pTypes = rOptions;
		notifyOptions->Flags = PRINTER_NOTIFY_OPTIONS_REFRESH;

		notifyInfo = NULL;
		m_HStatusNotify = FindFirstPrinterChangeNotification(m_HPrinterSpooler, 0, 0, notifyOptions);

		if (m_HStatusNotify)
		{
			while (m_HStatusNotify != INVALID_HANDLE_VALUE && !m_readTerminated)
			{
				if (WaitForSingleObject(m_HStatusNotify, INFINITE) == WAIT_OBJECT_0)
				{
					FindNextPrinterChangeNotification(m_HStatusNotify, &dwChanged, (LPVOID)notifyOptions, (LPVOID*)notifyInfo);

					updatePrinterStatus();
				}
			}

			if (m_HStatusNotify != INVALID_HANDLE_VALUE)
				FindClosePrinterChangeNotification(m_HStatusNotify);
		}

		return GetLastError();
	}

	DWORD PrinterSpooler::MonitorSpoolerJobs()
	{
		std::cout << "PrinterSpooler::MonitorSpoolerJobs()" << std::endl;

		PRINTER_NOTIFY_OPTIONS* notifyOptions;
		PPRINTER_NOTIFY_INFO notifyInfo;
		DWORD dwChanged;
		PRINTER_NOTIFY_OPTIONS_TYPE rOptions[1];
		bool m_Exited = false;

		WORD wJobFields[1];
		wJobFields[0] = JOB_NOTIFY_FIELD_STATUS;

		rOptions[0].Type = JOB_NOTIFY_TYPE;
		rOptions[0].pFields = &wJobFields[0];
		rOptions[0].Count = 1;

		notifyOptions = new PRINTER_NOTIFY_OPTIONS;
		notifyOptions->Count = 1;
		notifyOptions->Version = 2;
		notifyOptions->pTypes = rOptions;
		notifyOptions->Flags = PRINTER_NOTIFY_OPTIONS_REFRESH;

		notifyInfo = NULL;
		m_HJobNotify = FindFirstPrinterChangeNotification(m_HSpoolerJob, PRINTER_CHANGE_ALL, 0, notifyOptions);

		if (m_HJobNotify)
		{
			while (m_HJobNotify != INVALID_HANDLE_VALUE && !m_readTerminated)
			{
				if (WaitForSingleObject(m_HJobNotify, INFINITE) == WAIT_OBJECT_0)
				{	//Enters when the spooled job is PRINTED (sent completely to the printer) & DELETED from the spooler
					//The corresponding JobId NOT listed in the job list anymore!
					FindNextPrinterChangeNotification(m_HJobNotify, &dwChanged, (LPVOID)notifyOptions, (LPVOID*)&notifyInfo);

					if (notifyInfo == NULL)
					{
						std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> job notify info could NOT be received!" << std::endl;
						continue;
					}

					if (notifyInfo->Flags & PRINTER_NOTIFY_INFO_DISCARDED)
					{
						std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> PRINTER_NOTFIY_INFO discarded!" << std::endl;

						DWORD dwOldFlags = notifyOptions->Flags;
						notifyOptions->Flags = PRINTER_NOTIFY_OPTIONS_REFRESH;

						FreePrinterNotifyInfo(notifyInfo);
						FindNextPrinterChangeNotification(m_HJobNotify, &dwChanged, (LPVOID)notifyOptions, (LPVOID*)&notifyInfo);

						notifyOptions->Flags = dwOldFlags;
					}

					SpooledJobStatus_Map upToDateStatusMap;

					for (unsigned int i = 0; i < notifyInfo->Count; i++)
					{
						PRINTER_NOTIFY_INFO_DATA data = notifyInfo->aData[i];

						SpooledJobStatus currentStatus;
						if (data.Type == JOB_NOTIFY_TYPE)
						{
							if (data.Field == JOB_NOTIFY_FIELD_STATUS)
							{
								std::ostringstream o_str;

								if (data.NotifyData.adwData[0] & JOB_STATUS_SPOOLING)
								{
									//std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> status of Job:" << data.Id << " -> SPOOLING" << std::endl;
									currentStatus = SpooledJobStatus::Job_Spooling;
								}
								else if (data.NotifyData.adwData[0] & JOB_STATUS_PRINTING)
								{
									//std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> status of Job:" << data.Id << " -> PRINTING" << std::endl;
									currentStatus = SpooledJobStatus::Job_Printing;
								}
								else if (data.NotifyData.adwData[0] & JOB_STATUS_DELETING)
								{	//this case does not occur!
									//std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> status of Job:" << data.Id << " -> DELETING" << std::endl;
									currentStatus = SpooledJobStatus::Job_Deleting;
									//std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> status of Job:" << data.Id << " -> PRINTING" << std::endl;
									//currentStatus = SpooledJobStatus::Job_Printing;
								}
								else if (data.NotifyData.adwData[0] & JOB_STATUS_DELETED)
								{	//this case does not occur!
									//std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> status of Job:" << data.Id << " -> DELETED" << std::endl;
									currentStatus = SpooledJobStatus::Job_Deleted;
								}
								else if (data.NotifyData.adwData[0] & JOB_STATUS_PRINTED)
								{	//this case does not occur!
									//std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> status of Job:" << data.Id << " -> PRINTED" << std::endl;
									currentStatus = SpooledJobStatus::Job_Printed;
								}
								else if (data.NotifyData.adwData[0] & JOB_STATUS_ERROR)
								{	//this case does not occur!
									//std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> status of Job:" << data.Id << " -> ERROR" << std::endl;
									currentStatus = SpooledJobStatus::Job_Error;
								}
								else
								{
									//std::cout << "PrinterSpooler::MonitorSpoolerJobs() -> status of Job:" << data.Id << " -> UNKNOWN" << std::endl;
									currentStatus = SpooledJobStatus::Job_Unknown;
								}

								upToDateStatusMap.insert(SpooledJobStatus_Pair(data.Id, currentStatus));
							}
						}
					}

					updateSpooledJobStatus(upToDateStatusMap);
					upToDateStatusMap.clear();
				}
			}
		}

		return GetLastError();
	}

	DWORD PrinterSpooler::updatePrinterStatus()
	{
		std::lock_guard<std::mutex> lock(m_guard); //to make sure another thread should not enter here!!!

		DWORD dwNeeded = 0, dwPrintersR = 0, level = 2;
		PRINTER_INFO_2* prnInfo = NULL;

		SetLastError(0); //set the error as SUCCESS before starting operation!

		//get the required size for the PRINTERINFO_2 struct
		EnumPrinters(PRINTER_ENUM_NAME, NULL, level, NULL, 0, &dwNeeded, &dwPrintersR);

		prnInfo = (PRINTER_INFO_2 *)malloc(dwNeeded);

		//get the list of printers registered to the windows!
		if (EnumPrinters(PRINTER_ENUM_NAME, NULL, level, (LPBYTE)prnInfo, dwNeeded, &dwNeeded, &dwPrintersR) != FALSE)
		{
			for (unsigned int i = 0; i < dwPrintersR; i++)
			{
				//check whether the printer is the one we are looking for!
				if (strcmp(prnInfo[i].pPrinterName, m_printerName.c_str()) != 0)
					continue;

				if (prnInfo[i].Status == PRINTER_STATUS_OFFLINE ||
					prnInfo[i].Attributes & PRINTER_ATTRIBUTE_WORK_OFFLINE)
				{
					//std::cout << "PrinterSpooler::updatePrinterStatus() -> printer OFFLINE" << std::endl;
					m_status = PrinterStatus::Printer_Offline;
				}
				else if (prnInfo[i].Status == 0)
				{
					//std::cout << "PrinterSpooler::updatePrinterStatus() -> printer ONLINE" << std::endl;
					m_status = PrinterStatus::Printer_Ready;
				}
				else if (prnInfo[i].Status == PRINTER_STATUS_PAPER_JAM)
				{
					//std::cout << "PrinterSpooler::updatePrinterStatus() -> printer PAPERJAM" << std::endl;
					m_status = PrinterStatus::Printer_PaperJam;
				}
				else if (prnInfo[i].Status == PRINTER_STATUS_PAPER_OUT)
				{
					//std::cout << "PrinterSpooler::updatePrinterStatus() -> printer PAPEROUT" << std::endl;
					m_status = PrinterStatus::Printer_PaperOut;
				}
				else
				{
					//std::cout << "PrinterSpooler::updatePrinterStatus() -> printer in UNKNOWN status!" << std::endl;
					m_status = PrinterStatus::Printer_Unknown;
				}
				break; //if reached here, means we have found and checked status of the printer we are looking for!
			}
			free(prnInfo);
		}

		m_subscriber->on_printerStatusChange(m_status);

		return GetLastError();
	}

	void PrinterSpooler::updateSpooledJobStatus(const SpooledJobStatus_Map& upToDateStatusMap)
	{	//updates the job status and informs CUPPS_PM_OKI_PR object!
		std::lock_guard<std::mutex> lock(m_updateGuard);

		for (auto iter = m_jobStatusMap.begin(); iter != m_jobStatusMap.end(); )
		{
			bool willBeRemoved = false;
			auto upToDateIter = upToDateStatusMap.find(iter->first);
			if (upToDateIter != upToDateStatusMap.end())
			{
				if (upToDateIter->second == SpooledJobStatus::Job_Unknown || //can happen many times independent from the printer's current status! So, IGNORE!!!
					iter->second == SpooledJobStatus::Job_Deleting) //happens when the print job is cancelled by the user! Just wait for the print job to be removed from the spooler!
				{
					++iter;
					continue;
				}

				iter->second = upToDateIter->second;
			}
			else
			{	//the job is either Printed or Deleted !!!
				willBeRemoved = true;
				if (iter->second == Job_Printing)
					iter->second = Job_Printed;
				else if (iter->second == Job_Spooling || iter->second == Job_Deleting)
					iter->second = Job_Deleted;
				else
					iter->second = Job_Error;
			}

			m_subscriber->on_printDocumentStatusChange(iter->first, iter->second);

			if (willBeRemoved)
				iter = m_jobStatusMap.erase(iter); //remove from the map!
			else
				++iter;
		}
	}


	void PrinterSpooler::printDocuments(const PrintRequest_Map& printDocMap)
	{
		std::lock_guard<std::mutex> lock(m_guard);

		PrintDocResult_Map printResultMap;
		for (auto iter = printDocMap.begin(); iter != printDocMap.end(); iter++)
		{
			PrintDocumentResult printResult = sendDocumentToSpooler(iter->first, iter->second);
			printResultMap.insert(PrintDocResult_Pair(iter->first, printResult));
		}

		m_subscriber->on_printResponseReceived(printResultMap);
	}

	PrintDocumentResult PrinterSpooler::sendDocumentToSpooler(const std::string& documentName, const std::vector<std::string>& documentData)
	{	//creates a new Job on the spooler and sends the data

		PrintDocumentResult printResult = PrintDocumentResult::Print_OK;
		BYTE	pdBuffer[16384];
		PRINTER_INFO_2* pPrinterData;
		DWORD	cbBuf = sizeof(pdBuffer);
		DWORD	cbNeeded = 0;
		pPrinterData = (PRINTER_INFO_2 *)&pdBuffer[0];

		//retrieves information about the specified printer
		if (GetPrinter(m_HPrinterSpooler, 2, &pdBuffer[0], cbBuf, &cbNeeded) == TRUE)
		{
			//creates a device context (DC) for a device using the specified name
			HDC hdcPrint = CreateDC(szDriver, m_printerName.c_str(), pPrinterData->pPortName, NULL);

			if (hdcPrint)
			{
				int jobID = Escape(hdcPrint, STARTDOC, 8, documentName.c_str(), NULL);
				m_subscriber->on_feedBackJobId(jobID); //notified to CUPPS_PM_Device_PR_I instance object!

				if (jobID > 0)
				{
					m_jobStatusMap.insert(SpooledJobStatus_Pair(jobID, SpooledJobStatus::Job_Spooling));

					for (auto pageIter = documentData.begin(); pageIter != documentData.end(); pageIter++)
					{
						LPBITMAPINFO info; // Structure for storing the DIB information,
						HBITMAP hbit;  // it will be used by 'StretchDIBits()'
						BITMAP bm; // Handle to the bitmap to print
								   // about the bitmap (size, color depth...)
						int nColors = 0; // Used to store the number of colors the DIB has
						int sizeInfo = 0; // Will contain the size of info
						RGBQUAD rgb[256]; // Used to store the DIB color table


										  //loading Image data from string (string -> stream -> image)
						CImage image;
						IStream* imageStream = NULL;
						DWORD imageSize = pageIter->length();
						HGLOBAL hImage = ::GlobalAlloc(GMEM_MOVEABLE, imageSize);
						LPVOID pImage = ::GlobalLock(hImage);
						memcpy(pImage, pageIter->c_str(), imageSize);
						::CreateStreamOnHGlobal(hImage, FALSE, &imageStream);

						HRESULT loadRslt = image.Load(imageStream);
						if (loadRslt != S_OK)
						{
							imageStream->Release();
							GlobalUnlock(hImage);
							GlobalFree(hImage);
							printResult = PrintDocumentResult::NotReady;
							break;
						}
						hbit = image;
						GetObject(hbit, sizeof(BITMAP), (LPVOID)&bm);

						nColors = (1 << bm.bmBitsPixel);
						if (nColors > 256) // This is when DIB is 24 bit.
							nColors = 0; // In this case there is not any color table information

						// Now we need to know how much size we have to give for storing "info" in memory.
						// This involves the proper BITMAPINFO size and the color table size.
						// Color table is only needed when the DIB has 256 colors or less.
						sizeInfo = sizeof(BITMAPINFO) + (nColors * sizeof(RGBQUAD));   // This is the size required

						info = (LPBITMAPINFO)malloc(sizeInfo); // Storing info in memory

						// Before 'StretchDIBits()' we have to fill some "info" fields.
						// This information was stored in 'bm'.
						info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
						info->bmiHeader.biWidth = bm.bmWidth;
						info->bmiHeader.biHeight = bm.bmHeight;
						info->bmiHeader.biPlanes = 1;
						info->bmiHeader.biBitCount = bm.bmBitsPixel * bm.bmPlanes;
						info->bmiHeader.biCompression = BI_RGB;
						info->bmiHeader.biSizeImage = bm.bmWidthBytes * bm.bmHeight;
						info->bmiHeader.biXPelsPerMeter = 0;
						info->bmiHeader.biYPelsPerMeter = 0;
						info->bmiHeader.biClrUsed = 0;
						info->bmiHeader.biClrImportant = 0;

						// If the bitmap is a compressed bitmap (BI_RLE for example), the 'biSizeImage' can not
						// be calculated that way. A call to 'GetDIBits()' will fill the 'biSizeImage' field with
						// the correct size.

						// Now for 256 or less color DIB we have to fill the "info" color table parameter
						if (nColors <= 256)
						{
							HBITMAP hOldBitMap;
							HDC hMemDC = CreateCompatibleDC(NULL); // Creating an auxiliary device context

							hOldBitMap = (HBITMAP)SelectObject(hMemDC, hbit); // Select 'hbit' in 'it'
							GetDIBColorTable(hMemDC, 0, nColors, rgb); // Obtaining the color table information

																	   // Now we pass this color information to "info"
							for (int iCnt = 0; iCnt < nColors; ++iCnt)
							{
								info->bmiColors[iCnt].rgbRed = rgb[iCnt].rgbRed;
								info->bmiColors[iCnt].rgbGreen = rgb[iCnt].rgbGreen;
								info->bmiColors[iCnt].rgbBlue = rgb[iCnt].rgbBlue;
							}

							SelectObject(hMemDC, hOldBitMap);
							DeleteDC(hMemDC);
						}

						int destHeight = bm.bmHeight;
						int destWidth = bm.bmWidth;
						int topMargin = MARGIN;
						int leftMargin = MARGIN;

						if (bm.bmHeight > LETTER_HEIGHT)
							destHeight = (int)(LETTER_HEIGHT - 2 * MARGIN);
						else
							topMargin = (int)((LETTER_HEIGHT - bm.bmHeight) / 2 - MARGIN);

						if (bm.bmWidth > LETTER_WIDTH)
							destWidth = (int)(LETTER_WIDTH - 2 * MARGIN);
						else
							leftMargin = (int)((LETTER_WIDTH - bm.bmWidth) / 2 - MARGIN);

						//If fails => return value 0! Succeeds => non-zero!
						int dataSendRslt = StretchDIBits(hdcPrint,
							leftMargin, topMargin, destWidth, destHeight, //destination rectangle
							0, 0, bm.bmWidth, bm.bmHeight, //source rectangle
							bm.bmBits,
							info,
							DIB_RGB_COLORS,
							SRCCOPY);

						if (dataSendRslt == 0)
						{	//print failed!
							Escape(hdcPrint, ENDDOC, 0, NULL, NULL); //Remove the doc from the spooler
							printResult = PrintDocumentResult::NotReady;
							break;
						}

						int escRslt = Escape(hdcPrint, NEWFRAME, 0, NULL, NULL); //New Page

						if (escRslt <= 0)
						{	//Escape method failed!
							if (escRslt == SP_ERROR && GetLastError() == ERROR_PRINT_CANCELLED)
								printResult = PrintDocumentResult::Cancelled;
							else
								printResult = PrintDocumentResult::NotReady;
							break;
						}

						//releasing resources required to load image from string!
						imageStream->Release();
						GlobalUnlock(hImage);
						GlobalFree(hImage);

						//removing objects used for printing!
						DeleteObject(hbit);
						free(info);
					}

					//if not called, job stays on the spooler!
					Escape(hdcPrint, ENDDOC, 0, NULL, NULL); //Remove the doc from the spooler

					if (printResult == PrintDocumentResult::Print_OK)
					{
						if (m_status == Printer_Offline)
							printResult = PrintDocumentResult::NotReady; //print data sent to the spooler BUT the printer is OFFLINE!
						else if (m_status == Printer_PaperOut)
							printResult = PrintDocumentResult::PaperOut; //print data sent to the spooler BUT the printer is in PAPER_OUT status!
						else if (m_status == Printer_PaperJam)
							printResult = PrintDocumentResult::PaperJam; //print data sent to the spooler BUT the printer is in PAPER_JAM status!
					}
				}
				else
				{
					printResult = PrintDocumentResult::NotReady;
				}
			}
			else
			{
				printResult = PrintDocumentResult::NotReady;
			}

			DeleteObject(hdcPrint);
		}
		else
		{
			printResult = PrintDocumentResult::NotReady;
		}

		pPrinterData = NULL;

		return printResult;
	}

	PrintCancelResult PrinterSpooler::cancelPrintJob(unsigned long jobId)
	{
		JOB_INFO_2* pJobInfos = NULL;
		int cJobs;
		auto jobIter = m_jobStatusMap.find(jobId);
		PrintCancelResult cancelResult = PrintCancelResult::AlreadyCompleted;

		if (jobIter == m_jobStatusMap.end())
		{	//job does not exist in the job map => AlreadyCompleted!
			return cancelResult;
		}

		if (getPrintJobs(&pJobInfos, &cJobs) == TRUE)
		{
			for (int i = 0; i < cJobs; i++)
			{
				if (pJobInfos[i].JobId == jobId)
				{
					if (pJobInfos[i].pStatus != NULL && pJobInfos[i].Status == JOB_STATUS_DELETING)
					{
						cancelResult = PrintCancelResult::AlreadyCancelling;
					}
					else
					{
						SpooledJobStatus_Map  jobStatusMap;
						jobStatusMap.insert(SpooledJobStatus_Pair(jobId, SpooledJobStatus::Job_Deleting));
						updateSpooledJobStatus(jobStatusMap);

						SetJob(m_HPrinterSpooler, pJobInfos[i].JobId, 2, (LPBYTE)pJobInfos, JOB_CONTROL_DELETE);
						cancelResult = PrintCancelResult::Cancel_OK;
					}
					break;
				}
			}
		}

		if (pJobInfos != NULL)
			free(pJobInfos);
		pJobInfos = NULL;

		return cancelResult;
	}

	BOOL PrinterSpooler::getPrintJobs(JOB_INFO_2 **ppJobInfo, int *pcJobs)
	{
		DWORD cByteNeeded, nReturned, cByteUsed;
		JOB_INFO_2* pJobStorage = NULL;
		PRINTER_INFO_2* pPrinterInfo = NULL;

		if (!GetPrinter(m_HPrinterSpooler, 2, NULL, 0, &cByteNeeded)) //Get the buffer size needed.
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
				return FALSE;
		}

		pPrinterInfo = (PRINTER_INFO_2 *)malloc(cByteNeeded);
		if (!(pPrinterInfo))
		{
			return FALSE; //Failure to allocate memory.
		}

		if (!GetPrinter(m_HPrinterSpooler, 2, (LPBYTE)pPrinterInfo, cByteNeeded, &cByteUsed)) //Get the printer information.
		{	//Failure to access the printer.
			free(pPrinterInfo);
			pPrinterInfo = NULL;
			return FALSE;
		}

		if (!EnumJobs(m_HPrinterSpooler, 0, pPrinterInfo->cJobs, 2, NULL, 0, (LPDWORD)&cByteNeeded, (LPDWORD)&nReturned)) //Get job storage space
		{
			if (GetLastError() != ERROR_INSUFFICIENT_BUFFER)
			{
				free(pPrinterInfo);
				pPrinterInfo = NULL;
				return FALSE;
			}
		}

		pJobStorage = (JOB_INFO_2 *)malloc(cByteNeeded);
		if (!pJobStorage)
		{	//Failure to allocate Job storage space.
			free(pPrinterInfo);
			pPrinterInfo = NULL;
			return FALSE;
		}

		ZeroMemory(pJobStorage, cByteNeeded);

		if (!EnumJobs(m_HPrinterSpooler, 0, pPrinterInfo->cJobs, 2, (LPBYTE)pJobStorage, cByteNeeded, (LPDWORD)&cByteUsed, (LPDWORD)&nReturned)) //Get the list of jobs
		{
			free(pPrinterInfo);
			free(pJobStorage);
			pPrinterInfo = NULL;
			pJobStorage = NULL;
			return FALSE;
		}

		//Return the information
		*pcJobs = nReturned;
		*ppJobInfo = pJobStorage;

		free(pPrinterInfo);
		pPrinterInfo = NULL;

		return TRUE;
	}

	std::vector<std::string> PrinterSpooler::GetSpooledPrinters()
	{
		std::cout << "PrinterSpooler::GetSpooledPrinters() -> Listing the printers available in the system!" << std::endl;
		std::vector<std::string> spooledPrinters;
		DWORD dwNeeded = 0, dwPrintersR = 0, level = 2;
		PRINTER_INFO_2* prnInfo = NULL;

		SetLastError(0); //set the error as SUCCESS before starting operation!

						 //get the required size for the PRINTERINFO_2 struct
		EnumPrinters(PRINTER_ENUM_NAME, NULL, level, NULL, 0, &dwNeeded, &dwPrintersR);

		prnInfo = (PRINTER_INFO_2 *)malloc(dwNeeded);

		//get the list of printers registered to the windows!
		if (EnumPrinters(PRINTER_ENUM_NAME, NULL, level, (LPBYTE)prnInfo, dwNeeded, &dwNeeded, &dwPrintersR) != FALSE)
		{
			for (unsigned int i = 0; i < dwPrintersR; i++)
			{
				spooledPrinters.push_back(std::string(prnInfo[i].pPrinterName));
			}
			free(prnInfo);
		}
		return spooledPrinters;
	}

}
