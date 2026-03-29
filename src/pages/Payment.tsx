import { useLocation, useNavigate } from "react-router-dom";
import { CheckCircle, CreditCard, ArrowLeft, Loader2, AlertCircle } from "lucide-react";
import { Button } from "@/components/ui/button";
import { useState, useEffect } from "react";
import { initiateRazorpayPayment, loadRazorpayScript, RazorpayResponse } from "@/hooks/useRazorpay";
import { toast } from "@/hooks/use-toast";
import { decrementMultipleStock } from "@/lib/inventoryService";
import { MedicineWithPrice } from "@/lib/medicineData";
import { sendDispenseCommand } from "@/lib/dispenserService";

export default function Payment() {
  const location = useLocation();
  const navigate = useNavigate();
  const [isProcessing, setIsProcessing] = useState(false);
  const [isDispensing, setIsDispensing] = useState(false);
  const [isComplete, setIsComplete] = useState(false);
  const [isFailed, setIsFailed] = useState(false);
  const [paymentId, setPaymentId] = useState<string>("");
  const [sdkLoaded, setSdkLoaded] = useState(false);
  const [sdkError, setSdkError] = useState(false);

  const amount = location.state?.amount || 0;
  const items: MedicineWithPrice[] = location.state?.items || [];

  // Preload Razorpay SDK
  useEffect(() => {
    loadRazorpayScript().then((loaded) => {
      setSdkLoaded(loaded);
      if (!loaded) {
        setSdkError(true);
      }
    });
  }, []);

  const handlePayment = async () => {
    if (!amount || amount <= 0) {
      toast({
        title: "Invalid Amount",
        description: "Please go back and scan a valid prescription.",
        variant: "destructive",
      });
      return;
    }

    setIsProcessing(true);

    try {
      await initiateRazorpayPayment(
        amount,
        (response: RazorpayResponse) => {
          // Payment successful - decrement stock
          const stockItems = items.map((item) => ({
            name: item.name,
            qty: item.qty,
          }));
          decrementMultipleStock(stockItems);

          // Store payment ID and show dispensing UI
          setPaymentId(response.razorpay_payment_id);
          setIsProcessing(false);
          setIsDispensing(true);

          // Send dispense command to ESP32 hardware
          sendDispenseCommand(response.razorpay_payment_id, items)
            .then((result) => {
              if (result.success) {
                console.log("[Payment] ESP32 dispense successful:", result.dispensedCount, "tablets");
              } else {
                console.warn("[Payment] ESP32 dispense warning:", result.error);
                toast({
                  title: "Hardware Notice",
                  description: result.error || "Dispenser communication issue",
                  variant: "destructive",
                });
              }
            })
            .catch((err) => {
              console.error("[Payment] ESP32 dispense error:", err);
            })
            .finally(() => {
              // After dispensing completes (or fails), show thank you
              setTimeout(() => {
                setIsDispensing(false);
                setIsComplete(true);
              }, 1000);
            });
        },
        () => {
          // Payment dismissed/failed
          setIsProcessing(false);
          setIsFailed(true);
        }
      );
    } catch (error) {
      setIsProcessing(false);
      toast({
        title: "Payment Error",
        description: "Failed to initialize payment. Please try again.",
        variant: "destructive",
      });
    }
  };

  // Dispensing state
  if (isDispensing) {
    return (
      <div className="min-h-screen bg-background flex flex-col">
        <header className="bg-[image:var(--gradient-header)] px-6 py-4">
          <h1 className="text-2xl font-bold text-primary-foreground text-center">
            MediVend
          </h1>
        </header>

        <main className="flex-1 flex items-center justify-center p-6">
          <div className="text-center">
            <div className="w-24 h-24 rounded-full bg-primary/10 flex items-center justify-center mx-auto mb-6">
              <Loader2 className="w-14 h-14 text-primary animate-spin" />
            </div>
            <h2 className="text-3xl font-bold text-foreground mb-2">Dispensing Your Medicines</h2>
            <p className="text-muted-foreground text-lg mb-4">
              Please wait while we prepare your order...
            </p>
            <div className="flex justify-center gap-2 mt-6">
              <div className="w-3 h-3 rounded-full bg-primary animate-bounce" style={{ animationDelay: '0ms' }} />
              <div className="w-3 h-3 rounded-full bg-primary animate-bounce" style={{ animationDelay: '150ms' }} />
              <div className="w-3 h-3 rounded-full bg-primary animate-bounce" style={{ animationDelay: '300ms' }} />
            </div>
          </div>
        </main>
      </div>
    );
  }

  // Payment complete - Thank You
  if (isComplete) {
    return (
      <div className="min-h-screen bg-background flex flex-col">
        <header className="bg-[image:var(--gradient-header)] px-6 py-4">
          <h1 className="text-2xl font-bold text-primary-foreground text-center">
            MediVend
          </h1>
        </header>

        <main className="flex-1 flex items-center justify-center p-6">
          <div className="text-center">
            <div className="w-24 h-24 rounded-full bg-accent/10 flex items-center justify-center mx-auto mb-6 animate-pulse-glow">
              <CheckCircle className="w-14 h-14 text-accent" />
            </div>
            <h2 className="text-3xl font-bold text-foreground mb-2">Thank You!</h2>
            <p className="text-muted-foreground text-lg mb-4">
              Your medicines have been dispensed. Please collect them below.
            </p>
            <p className="text-5xl font-bold text-primary mb-4">₹{amount}</p>
            {paymentId && (
              <p className="text-sm text-muted-foreground mb-8">
                Transaction ID: <span className="font-mono">{paymentId}</span>
              </p>
            )}
            <Button
              onClick={() => navigate("/")}
              size="lg"
              className="h-14 px-8 text-lg font-semibold shadow-button"
            >
              <ArrowLeft className="w-5 h-5 mr-2" />
              Back to Scanner
            </Button>
          </div>
        </main>
      </div>
    );
  }

  // Payment failed state
  if (isFailed) {
    return (
      <div className="min-h-screen bg-background flex flex-col">
        <header className="bg-[image:var(--gradient-header)] px-6 py-4">
          <h1 className="text-2xl font-bold text-primary-foreground text-center">
            MediVend
          </h1>
        </header>

        <main className="flex-1 flex items-center justify-center p-6">
          <div className="text-center">
            <div className="w-24 h-24 rounded-full bg-destructive/10 flex items-center justify-center mx-auto mb-6">
              <AlertCircle className="w-14 h-14 text-destructive" />
            </div>
            <h2 className="text-3xl font-bold text-foreground mb-2">Payment Failed</h2>
            <p className="text-muted-foreground text-lg mb-8">
              Your payment could not be processed. Please try again.
            </p>
            <div className="flex flex-col gap-4">
              <Button
                onClick={() => {
                  setIsFailed(false);
                  handlePayment();
                }}
                size="lg"
                className="h-14 px-8 text-lg font-semibold shadow-button"
              >
                Retry Payment
              </Button>
              <Button
                variant="outline"
                onClick={() => navigate("/")}
                size="lg"
                className="h-14 px-8 text-lg font-semibold"
              >
                <ArrowLeft className="w-5 h-5 mr-2" />
                Back to Scanner
              </Button>
            </div>
          </div>
        </main>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-background flex flex-col">
      {/* Header */}
      <header className="bg-[image:var(--gradient-header)] px-6 py-4">
        <div className="flex items-center justify-between max-w-lg mx-auto">
          <Button
            variant="ghost"
            onClick={() => navigate("/")}
            className="text-primary-foreground hover:bg-primary-foreground/10"
          >
            <ArrowLeft className="w-5 h-5 mr-2" />
            Back
          </Button>
          <h1 className="text-2xl font-bold text-primary-foreground">Payment</h1>
          <div className="w-20" />
        </div>
      </header>

      <main className="flex-1 flex items-center justify-center p-6">
        <div className="w-full max-w-md">
          <div className="bg-card rounded-2xl shadow-card p-8">
            <div className="text-center mb-8">
              <div className="w-16 h-16 rounded-full bg-primary/10 flex items-center justify-center mx-auto mb-4">
                <CreditCard className="w-8 h-8 text-primary" />
              </div>
              <h2 className="text-xl font-bold text-foreground mb-1">Complete Payment</h2>
              <p className="text-muted-foreground">Secure payment via Razorpay</p>
            </div>

            <div className="bg-secondary rounded-xl p-6 text-center mb-8">
              <p className="text-muted-foreground text-sm mb-1">Amount to Pay</p>
              <p className="text-4xl font-bold text-primary">₹{amount}</p>
            </div>

            {sdkError && (
              <div className="mb-4 p-3 bg-destructive/10 border border-destructive/20 rounded-lg flex items-center gap-2">
                <AlertCircle className="w-5 h-5 text-destructive flex-shrink-0" />
                <p className="text-sm text-destructive">Failed to load payment gateway. Please refresh.</p>
              </div>
            )}

            <Button
              onClick={handlePayment}
              disabled={isProcessing || !sdkLoaded || sdkError || !amount}
              size="lg"
              className="w-full h-14 text-lg font-semibold shadow-button bg-accent hover:bg-accent/90"
            >
              {isProcessing ? (
                <>
                  <Loader2 className="w-5 h-5 mr-2 animate-spin" />
                  Processing...
                </>
              ) : !sdkLoaded && !sdkError ? (
                <>
                  <Loader2 className="w-5 h-5 mr-2 animate-spin" />
                  Loading...
                </>
              ) : (
                `Pay ₹${amount}`
              )}
            </Button>

            <p className="text-xs text-muted-foreground text-center mt-4">
              🔒 Secured by Razorpay • Test Mode
            </p>
          </div>
        </div>
      </main>
    </div>
  );
}
