#ifndef _SPOOLER_UTIL_H_
#define _SPOOLER_UTIL_H_

#include <string>
#include <iostream>
#include <map>


namespace Spooler
{
#if defined(_WIN32)
#   define DECLSPEC_EXPORT __declspec(dllexport)
#   define DECLSPEC_IMPORT __declspec(dllimport)
	//
	//  HAS_DECLSPEC_IMPORT_EXPORT defined only for compilers with distinct
	//  declspec for IMPORT and EXPORT
#   define HAS_DECLSPEC_IMPORT_EXPORT
#elif defined(__GNUC__)
#   define DECLSPEC_EXPORT __attribute__((visibility ("default")))
#   define DECLSPEC_IMPORT __attribute__((visibility ("default")))
#elif defined(__SUNPRO_CC)
#   define DECLSPEC_EXPORT __global
#   define DECLSPEC_IMPORT /**/
#else
#   define DECLSPEC_EXPORT /**/
#   define DECLSPEC_IMPORT /**/
#endif

#ifndef SPOOLER_DLL
#   ifdef SPOOLER_DLL_IMPORTS
#       define SPOOLER_DLL SPOOLER_DECLSPEC_IMPORT
#   elif defined(SPOOLER_STATIC_LIBS)
#       define SPOOLER_DLL /**/
#   else
#       define SPOOLER_DLL DECLSPEC_EXPORT
#   endif
#endif

	enum PrinterStatus
	{
		Printer_Ready,
		Printer_Offline,
		Printer_PaperOut,
		Printer_PaperJam,
		Printer_Unknown
	};

	static std::ostream& operator<<(std::ostream& outStream, PrinterStatus status)
	{
		switch (status)
		{
		case Printer_Ready:
			outStream << "Ready";
			break;
		case Printer_Offline:
			outStream << "Offline";
			break;
		case Printer_PaperOut:
			outStream << "PaperOut";
			break;
		case Printer_PaperJam:
			outStream << "PaperJam";
			break;
		default:
			outStream << "Unknown";
			break;
		}
		return outStream;
	}

	enum SpooledJobStatus
	{
		Job_Spooling,	//sending print data to the spooler from the application
		Job_Printing,	//sending print data to the printer from the spooler
		Job_Printed,	//print data sent to the printer completely
		Job_Deleting,	//deleting the job from the spooler
		Job_Deleted,	//job is deleted from the spooler
		Job_Error,		//error happened for the spooled job
		Job_Unknown		//status for the job is unknown
	};

	static std::ostream& operator<<(std::ostream& outStream, SpooledJobStatus status)
	{
		switch (status)
		{
		case Job_Spooling:
			outStream << "Spooling";
			break;
		case Job_Printing:
			outStream << "Printing";
			break;
		case Job_Printed:
			outStream << "Printed";
			break;
		case Job_Deleting:
			outStream << "Deleting";
			break;
		case Job_Deleted:
			outStream << "Deleted";
			break;
		case Job_Error:
			outStream << "Error";
			break;
		default:
			outStream << "Unknown";
			break;
		}
		return outStream;
	}

	enum PrintDocumentResult
	{
		NotReady,	//spooling the document fails || the printer is OFFLINE
		PaperJam,	//paper is jammed on the printer
		PaperOut,	//the printer is out of paper
		Cancelled,	//if Escape(...) function returns SP_ERROR && GetLastError()==ERROR_PRINT_CANCELLED
		Print_OK
	};

	static std::ostream& operator<<(std::ostream& outStream, PrintDocumentResult result)
	{
		switch (result)
		{
		case NotReady:
			outStream << "NotReady";
			break;
		case PaperJam:
			outStream << "PaperJam";
			break;
		case PaperOut:
			outStream << "PaperOut";
			break;
		case Cancelled:
			outStream << "Cancelled";
			break;
		case Print_OK:
			outStream << "Print OK";
			break;
		default:
			outStream << "Unknown";
			break;
		}
		return outStream;
	}

	enum PrintCancelResult
	{
		InvalidDocumentName,
		AlreadyCancelling,
		AlreadyCancelled,
		AlreadyCompleted,
		Cancel_OK
	};

	struct PrintDocumentInfo
	{
		SPOOLER_DLL PrintDocumentInfo(const std::string& documentName) :
			m_documentName(documentName), m_status(Job_Spooling) {}

		std::string m_documentName;
		SpooledJobStatus m_status;
	};
	using PrintedDocStatus_Pair = std::pair<unsigned long, PrintDocumentInfo>; // <jobId, printDocumentInfo>
	using PrintedDocStatus_Map = std::map<unsigned long, PrintDocumentInfo>;
	
}

#endif
