from sklearn.gaussian_process import GaussianProcessRegressor
from sklearn.gaussian_process.kernels import RBF, ConstantKernel as C
from sklearn.neural_network import MLPRegressor
from sklearn.svm import SVR
from sklearn.model_selection import train_test_split, GridSearchCV
from sklearn.preprocessing import PolynomialFeatures
from sklearn.preprocessing import StandardScaler
from sklearn.linear_model import LinearRegression
from sklearn.pipeline import make_pipeline
from sklearn.metrics import mean_squared_error

import warnings
from sklearn.exceptions import ConvergenceWarning
from scipy.linalg import LinAlgWarning

def create(model_type, seed=42, poly_degree=2, poly_nnls=False):
  # Model selection
  if model_type == "poly":
    model = make_pipeline(
      PolynomialFeatures(poly_degree), LinearRegression(positive=poly_nnls)
    )
  elif model_type == "gpr":
    # Define the kernel
    kernel = C(1.0, (1e-5, 1e8)) * RBF(1.0, (1e-6, 1e2))

    # Instantiate the model with the kernel
    model = GaussianProcessRegressor(kernel=kernel, n_restarts_optimizer=10)
  elif model_type == "nn":
    # Define a Neural Network model
    # Note: You may need to adjust the architecture and parameters depending on your specific dataset
    model = MLPRegressor(
      hidden_layer_sizes=(100, 50),
      activation="logistic",  # Rectified Linear Unit activation function
      solver="lbfgs",  # Default solver for weight optimization
      learning_rate_init=0.01,  # Default value, consider adjusting
      max_iter=1000,  # Maximum number of iterations
      random_state=seed,
      early_stopping=False,
    )
  elif model_type == "svm":
    # Note: The default kernel is 'rbf' (Radial Basis Function), which is commonly used for non-linear data.
    # You may need to adjust the kernel, C, epsilon, and other parameters depending on your specific dataset.
    model = SVR(kernel="rbf", C=1e3, epsilon=0.1)
  else:
    raise ValueError("Unsupported model type. Choose 'poly', 'nn', 'gpr', or 'svm'.")

  return model


def fit(
    model,
    input,
    output,
    suppress_warnings : bool = True,
  ):
  # Training the model
  if suppress_warnings:
    with warnings.catch_warnings():
      warnings.simplefilter("ignore", ConvergenceWarning)
      warnings.simplefilter("ignore", LinAlgWarning)

      model.fit(input[-30:], output[-30:])
  else:
    model.fit(input[-30:], output[-30:])

def predict(model, x):
  return list(model.predict(x))

if __name__ == "__main__":
    main()
