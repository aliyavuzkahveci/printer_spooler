/*
@author  Ali Yavuz Kahveci aliyavuzkahveci@gmail.com
* @version 1.0
* @since   19-07-2018
* @Purpose: tester class to use the Printer Spooler object
*/

#include <PrinterSpooler.h>

using namespace Spooler;

namespace SpoolerTest
{
	class SpoolerTester : public Spooler_Subscriber
	{
	public:
		SpoolerTester();

		virtual ~SpoolerTester();

		std::vector<std::string> getSpooledPrinters();

		void connectToPrinter(const std::string& printerName);

		void disconnectFromPrinter();

		void startPrinting(const std::string& filePath);

	private:
		void on_printerStatusChange(PrinterStatus newStatus) override;

		void on_printDocumentStatusChange(unsigned long jobId, SpooledJobStatus status) override;

		void on_feedBackJobId(const unsigned long jobId) override;

		void on_printResponseReceived(const PrintDocResult_Map&) override;

		//param 1 -> filePath, param 2 -> the content of the file given in param 1
		//return value -> the size of the file data
		unsigned int readPNGFiles(const std::string&, char**);

		PrinterSpooler_Ptr m_spooler;
		PrinterStatus m_currentStatus;

	};
	using SpoolerTester_Ptr = std::shared_ptr<SpoolerTester>;

}
