import numpy as np

class Layer:
    def forward(self, input):
        """
        Forward pass
        Args:
            input: input data
        Returns:
            output: output data
        """
        raise NotImplementedError
    
    def backward(self, grad_output):
        """
        Backward pass
        Args:
            grad_output: gradient of loss w.r.t. output of this layer
        Returns:
            grad_input: gradient of loss w.r.t. input of this layer
        """
        raise NotImplementedError


class Linear(Layer):
    # TODO
    def __init__(self, in_features, out_features):
        """
        Args:
            in_features: number of input features
            out_features: number of output features
        """
        # TODO
        return
    
    def forward(self, input):
        """
        Forward pass: y = xW^T + b
        Args:
            input: (batch_size, in_features)
        Returns:
            output: (batch_size, out_features)
        """
        # TODO
        return
    
    def backward(self, grad_output):
        """
        Backward pass
        Args:
            grad_output: (batch_size, out_features)
        Returns:
            grad_input: (batch_size, in_features)
        """
        # TODO
        return

class ReLU(Layer):
    def __init__(self):
        # TODO
        return
    
    def forward(self, input):
        """
        Forward pass:
        Args:
            input: any shape
        Returns:
            output: same shape as input
        """
        # TODO
        return 
    
    def backward(self, grad_output):
        """
        Backward pass
        Args:
            grad_output: same shape as input
        Returns:
            grad_input: same shape as input
        """
        # TODO
        return 
    
def softmax(x):
    # TODO
    return 

class SimpleNetwork:
    """
    Simple 3-layer neural network
    Architecture: Linear -> ReLU -> Linear -> ReLU -> Linear
    """
    def __init__(self, input_size, hidden_size1, hidden_size2, output_size):
        # TODO
        pass
    
    def forward(self, x):
        # TODO
        return 
    
    def backward(self, grad_output):
        # TODO
        return
    
    
def train_with_gd(network, X_train, y_train, learning_rate, num_epochs):
    # TODO
    losses = []
    return losses


if __name__ == "__main__":
    np.random.seed(42)
    
    train_all = np.loadtxt('data/smallTrain.csv', dtype=int, delimiter=',')
    X_train = train_all[:, 1:]  
    y_train = train_all[:, 0]   

    valid_all = np.loadtxt('data/smallValidation.csv', dtype=int, delimiter=',')
    X_val = valid_all[:, 1:]   
    y_val = valid_all[:, 0]   

    input_size = 128    
    hidden_size1 = 128  
    hidden_size2 = 64   
    output_size = 10   

    network = SimpleNetwork(input_size, hidden_size1, hidden_size2, output_size)
    
    # Training parameters
    learning_rate = 0.1
    num_epochs = 100
    losses = train_with_gd(network, X_train, y_train, learning_rate, num_epochs)

    print(f"\nFinal Results:")

    # Training set evaluation
    logits_train = network.forward(X_train)
    probs_train = softmax(logits_train)
    predictions_train = np.argmax(probs_train, axis=1)
    accuracy_train = np.mean(predictions_train == y_train)

    print(f"  Training Loss: {losses[-1]:.4f}")
    print(f"  Training Accuracy: {accuracy_train:.2%}")

    # Validation set evaluation
    logits_val = network.forward(X_val)
    probs_val = softmax(logits_val)
    predictions_val = np.argmax(probs_val, axis=1)
    accuracy_val = np.mean(predictions_val == y_val)

    print(f"\n  Validation Accuracy: {accuracy_val:.2%}")