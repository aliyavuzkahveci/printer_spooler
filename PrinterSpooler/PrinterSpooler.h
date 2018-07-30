#ifndef _PRINTER_SPOOLER_H_
#define _PRINTER_SPOOLER_H_

/*@author  Ali Yavuz Kahveci aliyavuzkahveci@gmail.com
* @version 1.0
* @since   19-07-2018
* @Purpose: printer spooler operations handler for the standard printers
!!!WARNING -> this class implementation depends on WINDOWS,
so it will not work on Linux environment!!!
*/

#include <memory>
#include <vector>
#include <queue>
#include <mutex>

#include <windows.h>
#include <winspool.h>
#include <atlimage.h>
#include <ObjIdl.h>

#include "SpoolerUtil.h"

namespace Spooler
{
	using PrintDocResult_Pair = std::pair<std::string, PrintDocumentResult>; // <documentName, printResult>
	using PrintDocResult_Map = std::map<std::string, PrintDocumentResult>;
	//using PrintDocResult_Iter = PrintDocResult_Map::iterator;
	//using PrintDocResult_ConstIter = PrintDocResult_Map::const_iterator;

	using SpooledJobStatus_Pair = std::pair<unsigned long, SpooledJobStatus>; // <jobId, jobStatus>
	using SpooledJobStatus_Map = std::map<unsigned long, SpooledJobStatus>;
	//using SpooledJobStatus_Iter = SpooledJobStatus_Map::iterator;
	//using SpooledJobStatus_ConstIter = SpooledJobStatus_Map::const_iterator;

	using PrintRequest_Pair = std::pair<std::string, std::vector<std::string>>; // <documentName, documentData>
	using PrintRequest_Map = std::map<std::string, std::vector<std::string>>;
	//using PrintRequest_Iter = PrintRequest_Map::iterator;
	//using PrintRequest_ConstIter = PrintRequest_Map::const_iterator;

	class Spooler_Subscriber;
	using Spooler_Subscriber_Ptr = std::shared_ptr<Spooler_Subscriber>;

	class SPOOLER_DLL PrinterSpooler
	{
	public:
		PrinterSpooler(const Spooler_Subscriber_Ptr& observer, const std::string& printerName);

		virtual ~PrinterSpooler();

		void printDocuments(const PrintRequest_Map&);

		void initializePrinterSpooler();

		void closePrinterSpooler();

		static std::vector<std::string> GetSpooledPrinters();

	private:
		/*to protect the class from being copied*/
		PrinterSpooler(const PrinterSpooler&) = delete;
		PrinterSpooler& operator=(const PrinterSpooler&) = delete;
		PrinterSpooler(PrinterSpooler&&) = delete;
		/*to protect the class from being copied*/

		DWORD updatePrinterStatus();

		/* Printer Status Monitor Thread */
		static DWORD WINAPI startMonitorPrinter(LPVOID lpV);
		DWORD MonitorPrinter();

		/* Spooler Jobs Status Monitor Thread */
		static DWORD WINAPI startMonitorSpoolerJobs(LPVOID lpV);
		DWORD MonitorSpoolerJobs();

		void updateSpooledJobStatus(const SpooledJobStatus_Map&);

		PrintDocumentResult sendDocumentToSpooler(const std::string&, const std::vector<std::string>&);

		PrintCancelResult cancelPrintJob(unsigned long jobId);

		/*Get the state information for the Printer Queue and the jobs in the Printer Queue.
		params: ppJobInfo -> job list in the printer queue, pcJobs -> # of jobs in the queue */
		BOOL getPrintJobs(JOB_INFO_2 **ppJobInfo, int *pcJobs);

		Spooler_Subscriber_Ptr m_subscriber;

		HANDLE m_HPrinterSpooler = NULL; //handle for printer spooler
		HANDLE m_HSpoolerJob = NULL; //handle for printer to be notified about the status of the spooler jobs!
		HANDLE m_HStatusThread = NULL; //handle for printer status thread
		HANDLE m_HJobStatusThread = NULL; //handle for job status thread
		HANDLE m_HStatusNotify = NULL; //handle for printer status (online, offline, paperOut) notify
		HANDLE m_HJobNotify = NULL; //handle for job status (spooling, printing, etc.) notify

		std::mutex m_guard;
		std::mutex m_updateGuard;

		SpooledJobStatus_Map m_jobStatusMap;
		PrinterStatus m_status;

		std::string m_printerName;
		bool m_openSuccess;
		bool m_readTerminated;

	};
	using PrinterSpooler_Ptr = std::shared_ptr<PrinterSpooler>;

	//must be inherited by all the Printer Devices to be able to communicate with the Spooler!
	class SPOOLER_DLL Spooler_Subscriber : public std::enable_shared_from_this<Spooler_Subscriber>
	{
		friend class PrinterSpooler;
	public:
		Spooler_Subscriber() {}

		virtual ~Spooler_Subscriber() {}

	protected:
		virtual void on_printerStatusChange(PrinterStatus newStatus) = 0;

		virtual void on_printDocumentStatusChange(unsigned long jobId, SpooledJobStatus status) = 0;

		virtual void on_feedBackJobId(const unsigned long jobId) = 0;

		virtual void on_printResponseReceived(const PrintDocResult_Map&) = 0;

		PrintedDocStatus_Map m_printDocInfoMap;
		std::queue<PrintDocumentInfo> m_waitingForJobIdDocs;
		std::vector<std::string> m_cancelledDocumentNames;

	};

}

#endif
