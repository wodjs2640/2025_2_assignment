from model import SimplifiedResNet, LeNet5
from util import get_config, plot_result
from train import train_model

def main():
    config = get_config()
    for param_name, value in config.items():
        print(f"{param_name}: {value}")
        
    exp_result_dict = {}
    
    for model_class in [SimplifiedResNet, LeNet5]:
        model_name = model_class.__name__
        
        print("="*60)
        print(f"{model_name} Training on CIFAR-10")
        print("="*60)
    
        # Create model
        model = model_class()
        
        # Train
        history = train_model(
            model, config
        )
        exp_result_dict[model_name] = history
        
        print("\n" + "="*60)
        print("Training completed successfully!")
        print("="*60)
    
    # Plot training curves
    plot_result(exp_result_dict, 'model_comparison_results')

if __name__ == "__main__":
    main()